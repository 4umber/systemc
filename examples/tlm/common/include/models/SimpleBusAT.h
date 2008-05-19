/*****************************************************************************

  The following code is derived, directly or indirectly, from the SystemC
  source code Copyright (c) 1996-2008 by all Contributors.
  All Rights reserved.

  The contents of this file are subject to the restrictions and limitations
  set forth in the SystemC Open Source License Version 3.0 (the "License");
  You may not use this file except in compliance with such restrictions and
  limitations. You may obtain instructions on how to receive a copy of the
  License at http://www.systemc.org/. Software distributed by Contributors
  under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
  ANY KIND, either express or implied. See the License for the specific
  language governing rights and limitations under the License.

 *****************************************************************************/

#ifndef __SIMPLEBUSAT_H__
#define __SIMPLEBUSAT_H__

//#include <systemc>
#include "tlm.h"

#include "simple_target_socket.h"
#include "simple_initiator_socket.h"

#include "PEQFifo.h"

template <int NR_OF_INITIATORS, int NR_OF_TARGETS>
class SimpleBusAT : public sc_core::sc_module
{
public:
  typedef tlm::tlm_generic_payload               transaction_type;
  typedef tlm::tlm_phase                         phase_type;
  typedef tlm::tlm_sync_enum                     sync_enum_type;
  typedef SimpleTargetSocketTagged<SimpleBusAT>    target_socket_type;
  typedef SimpleInitiatorSocketTagged<SimpleBusAT> initiator_socket_type;

public:
  target_socket_type target_socket[NR_OF_INITIATORS];
  initiator_socket_type initiator_socket[NR_OF_TARGETS];

public:
  SC_HAS_PROCESS(SimpleBusAT);
  SimpleBusAT(sc_core::sc_module_name name) :
    sc_core::sc_module(name),
    mRequestPEQ("requestPEQ"),
    mResponsePEQ("responsePEQ")
  {
     for (unsigned int i = 0; i < NR_OF_INITIATORS; ++i) {
       target_socket[i].registerNBTransport(this, &SimpleBusAT::initiatorNBTransport, i);
       target_socket[i].registerDebugTransport(this, &SimpleBusAT::transportDebug, i);
       target_socket[i].registerDMI(this, &SimpleBusAT::getDMIPointer, i);
     }
     for (unsigned int i = 0; i < NR_OF_TARGETS; ++i) {
       initiator_socket[i].registerNBTransport_bw(this, &SimpleBusAT::targetNBTransport, i);
       initiator_socket[i].registerInvalidateDMI(this, &SimpleBusAT::invalidateDMIPointers, i);
     }

     SC_THREAD(RequestThread);
     SC_THREAD(ResponseThread);
  }

  //
  // Dummy decoder:
  // - address[31-28]: portId
  // - address[27-0]: masked address
  //

  unsigned int getPortId(const sc_dt::uint64& address)
  {
    return (unsigned int)address >> 28;
  }

  sc_dt::uint64 getAddressOffset(unsigned int portId)
  {
    return portId << 28;
  }

  sc_dt::uint64 getAddressMask(unsigned int portId)
  {
    return 0xfffffff;
  }

  unsigned int decode(const sc_dt::uint64& address)
  {
    // decode address:
    // - return initiator socket id

    return getPortId(address);
  }

  //
  // AT protocol
  //

  void RequestThread()
  {
    while (true) {
      wait(mRequestPEQ.getEvent());

      transaction_type* trans;
      while ((trans = mRequestPEQ.getNextTransaction())!=0) {
        unsigned int portId = decode(trans->get_address());
        assert(portId < NR_OF_TARGETS);
        initiator_socket_type* decodeSocket = &initiator_socket[portId];
        trans->set_address(trans->get_address() & getAddressMask(portId));

        // Fill in the destination port
        PendingTransactionsIterator it = mPendingTransactions.find(trans);
        assert(it != mPendingTransactions.end());
        it->second.to = decodeSocket;

        phase_type phase = tlm::BEGIN_REQ;
        sc_core::sc_time t = sc_core::SC_ZERO_TIME;

        // FIXME: No limitation on number of pending transactions
        //        All targets (that return false) must support multiple transactions
        switch ((*decodeSocket)->nb_transport_fw(*trans, phase, t)) {
        case tlm::TLM_ACCEPTED:
        case tlm::TLM_UPDATED:
          // Transaction not yet finished
          if (phase == tlm::BEGIN_REQ) {
            // Request phase not yet finished
            wait(mEndRequestEvent);

          } else if (phase == tlm::END_REQ) {
            // Request phase finished, but response phase not yet started
            wait(t);
            phase = tlm::END_REQ;
            t = sc_core::SC_ZERO_TIME;
            (*it->second.from)->nb_transport_bw(*trans, phase, t);

          } else if (phase == tlm::BEGIN_RESP) {
            mResponsePEQ.notify(*trans, t);
            continue;

          } else { // END_RESP
            assert(0); exit(1);
          }

          break;

        case tlm::TLM_COMPLETED:
          // Transaction finished
          mResponsePEQ.notify(*trans, t);

          // reset to destination port (we must not send END_RESP to target)
          it->second.to = 0;

          wait(t);
          break;

        default:
          assert(0); exit(1);
        };
      }
    }
  }

  void ResponseThread()
  {
    while (true) {
      wait(mResponsePEQ.getEvent());

      transaction_type* trans;
      while ((trans = mResponsePEQ.getNextTransaction())!=0) {
        PendingTransactionsIterator it = mPendingTransactions.find(trans);
        assert(it != mPendingTransactions.end());

        phase_type phase = tlm::BEGIN_RESP;
        sc_core::sc_time t = sc_core::SC_ZERO_TIME;

        switch ((*it->second.from)->nb_transport_bw(*trans, phase, t)) {
        case tlm::TLM_COMPLETED:
          // Transaction finished

          // Transaction may already be deleted (or re-used)
          // --> immediately notify target
          if (it->second.to) {
            phase = tlm::END_RESP;
            bool r = (*it->second.to)->nb_transport_fw(*trans, phase, t);
            assert(r);
          }
          mPendingTransactions.erase(it);
          wait(t);
          break;

        case tlm::TLM_ACCEPTED:
        case tlm::TLM_UPDATED:
          // Transaction not yet finished
          wait(mEndResponseEvent);
        break;

        default:
          assert(0); exit(1);
        };
      }
    }
  }

  //
  // interface methods
  //

  sync_enum_type initiatorNBTransport(int initiator_id,
                                      transaction_type& trans,
                                      phase_type& phase,
                                      sc_core::sc_time& t)
  {
    if (phase == tlm::BEGIN_REQ) {
      addPendingTransaction(trans, 0, initiator_id);

      mRequestPEQ.notify(trans, t);

    } else if (phase == tlm::END_RESP) {
      // after returning true the transaction can be deleted (or re-used)
      // --> immediately forward call to target
      PendingTransactionsIterator it = mPendingTransactions.find(&trans);
      assert(it != mPendingTransactions.end());

      if (it->second.to) {
        phase = tlm::END_RESP;
        sync_enum_type r = (*it->second.to)->nb_transport_fw(trans, phase, t);
        assert(r == tlm::TLM_COMPLETED);
      }

      mPendingTransactions.erase(it);

      mEndResponseEvent.notify(t);
      return tlm::TLM_COMPLETED;

    } else {
      std::cout << "ERROR: '" << name()
                << "': Illegal phase received from initiator." << std::endl;
      assert(false); exit(1);
    }

    return tlm::TLM_ACCEPTED;
  }

  sync_enum_type targetNBTransport(int portId,
                                   transaction_type& trans,
                                   phase_type& phase,
                                   sc_core::sc_time& t)
  {
    if (phase != tlm::END_REQ && phase != tlm::BEGIN_RESP) {
      std::cout << "ERROR: '" << name()
                << "': Illegal phase received from target." << std::endl;
      assert(false); exit(1);
    }

    mEndRequestEvent.notify(t);
    if (phase == tlm::END_REQ) {
      PendingTransactionsIterator it = mPendingTransactions.find(&trans);
      assert(it != mPendingTransactions.end());
      (*it->second.from)->nb_transport_bw(trans, phase, t);

    } else if (phase == tlm::BEGIN_RESP) {
      mResponsePEQ.notify(trans, t);
    }

    return tlm::TLM_ACCEPTED;
  }

  unsigned int transportDebug(int initiator_id, transaction_type& trans)
  {
    unsigned int portId = decode(trans.get_address());
    assert(portId < NR_OF_TARGETS);
    initiator_socket_type* decodeSocket = &initiator_socket[portId];
    trans.set_address( trans.get_address() & getAddressMask(portId) );
    
    return (*decodeSocket)->transport_dbg(trans);
  }

  bool limitRange(unsigned int portId, sc_dt::uint64& low, sc_dt::uint64& high)
  {
    sc_dt::uint64 addressOffset = getAddressOffset(portId);
    sc_dt::uint64 addressMask = getAddressMask(portId);

    if (low > addressMask) {
      // Range does not overlap with addressrange for this target
      return false;
    }

    low += addressOffset;
    if (high > addressMask) {
      high = addressOffset + addressMask;

    } else {
      high += addressOffset;
    }
    return true;
  }

  bool getDMIPointer(int initiator_id,
                     transaction_type& trans,
                     tlm::tlm_dmi&  dmi_data)
  {
    // FIXME: DMI not supported for AT bus?
    sc_dt::uint64 address = trans.get_address();

    unsigned int portId = decode(address);
    assert(portId < NR_OF_TARGETS);
    initiator_socket_type* decodeSocket = &initiator_socket[portId];
    sc_dt::uint64 maskedAddress = address & getAddressMask(portId);

    trans.set_address(maskedAddress);

    bool result =
      (*decodeSocket)->get_direct_mem_ptr(trans, dmi_data);
    
    if (result)
    {
      // Range must contain address
      assert(dmi_data.get_start_address() <= maskedAddress);
      assert(dmi_data.get_end_address() >= maskedAddress);
    }
    
    // Should always succeed
	sc_dt::uint64 start, end;
	start = dmi_data.get_start_address();
	end = dmi_data.get_end_address();
	
	limitRange(portId, start, end);
	
	dmi_data.set_start_address(start);
	dmi_data.set_end_address(end);

    return result;
  }

  void invalidateDMIPointers(int portId,
                             sc_dt::uint64 start_range,
                             sc_dt::uint64 end_range)
  {
    // FIXME: probably faster to always invalidate everything?

    if ((portId >= 0) && !limitRange(portId, start_range, end_range)) {
      // Range does not fall into address range of target
      return;
    }
    
    for (unsigned int i = 0; i < NR_OF_INITIATORS; ++i) {
      (target_socket[i])->invalidate_direct_mem_ptr(start_range, end_range);
    }
  }

private:
  void addPendingTransaction(transaction_type& trans,
                             initiator_socket_type* to,
                             int initiatorId)
  {
    const ConnectionInfo info = { &target_socket[initiatorId], to };
    assert(mPendingTransactions.find(&trans) == mPendingTransactions.end());
    mPendingTransactions[&trans] = info;
  }

private:
  struct ConnectionInfo {
    target_socket_type* from;
    initiator_socket_type* to;
  };
  typedef std::map<transaction_type*, ConnectionInfo> PendingTransactions;
  typedef typename PendingTransactions::iterator PendingTransactionsIterator;
  typedef typename PendingTransactions::const_iterator PendingTransactionsConstIterator;

private:
  PendingTransactions mPendingTransactions;

  PEQFifo mRequestPEQ;
  sc_core::sc_event mBeginRequestEvent;
  sc_core::sc_event mEndRequestEvent;

  PEQFifo mResponsePEQ;
  sc_core::sc_event mBeginResponseEvent;
  sc_core::sc_event mEndResponseEvent;
};

#endif
