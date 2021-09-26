#include "sysc/communication/sc_communication_ids.h"
#include "sysc/utils/sc_utils_ids.h"
#include "sysc/communication/sc_callback_signal.h"
#include "sysc/kernel/sc_reset.h"
#include "sysc/kernel/sc_simcontext_int.h"

#include <sstream>

using sc_dt::sc_logic;

namespace sc_core {

template <sc_writer_policy POL>
void sc_callback_signal<bool, POL>::update() {
    policy_type::update();
    if (!(base_type::m_new_val == base_type::m_cur_val)) {
        do_update();
    }
}

template <sc_writer_policy POL>
void sc_callback_signal<bool, POL>::do_update() {
    // order of execution below is important, the notify_processes() call
    // must come after the update of m_cur_val for things to work properly!
    base_type::do_update();
    if (m_reset_p)
        m_reset_p->notify_processes();

    sc_event *event_p = this->m_cur_val ? m_posedge_event_p : m_negedge_event_p;
    sc_signal_channel::notify_next_delta(event_p);
}

// edge event methods

template <sc_writer_policy POL>
const sc_event &sc_callback_signal<bool, POL>::posedge_event() const {
    return *this->lazy_kernel_event(&m_posedge_event_p, "posedge_event");
}

template <sc_writer_policy POL>
const sc_event &sc_callback_signal<bool, POL>::negedge_event() const {
    return *this->lazy_kernel_event(&m_negedge_event_p, "negedge_event");
}

// reset support

template <sc_writer_policy POL>
sc_reset *sc_callback_signal<bool, POL>::is_reset() const {
    sc_reset *result_p;
    if (!m_reset_p)
        m_reset_p = new sc_reset(this);
    result_p = m_reset_p;
    return result_p;
}

// destructor

template <sc_writer_policy POL>
sc_callback_signal<bool, POL>::~sc_callback_signal() {
    delete m_negedge_event_p;
    delete m_posedge_event_p;
    delete m_reset_p;
}

// IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII

template <sc_writer_policy POL>
void sc_callback_signal<sc_logic, POL>::update() {
    policy_type::update();
    if (!(base_type::m_new_val == base_type::m_cur_val)) {
        do_update();
    }
}

template <sc_writer_policy POL>
void sc_callback_signal<sc_logic, POL>::do_update() {
    base_type::do_update();

    if (this->m_cur_val == sc_dt::SC_LOGIC_1) {
        sc_signal_channel::notify_next_delta(m_posedge_event_p);
    } else if (this->m_cur_val == sc_dt::SC_LOGIC_0) {
        sc_signal_channel::notify_next_delta(m_negedge_event_p);
    }
}

// edge event methods

template <sc_writer_policy POL>
const sc_event &sc_callback_signal<sc_logic, POL>::posedge_event() const {
    return *this->lazy_kernel_event(&m_posedge_event_p, "posedge_event");
}

template <sc_writer_policy POL>
const sc_event &sc_callback_signal<sc_logic, POL>::negedge_event() const {
    return *this->lazy_kernel_event(&m_negedge_event_p, "negedge_event");
}

// destructor

template <sc_writer_policy POL>
sc_callback_signal<sc_logic, POL>::~sc_callback_signal() {
    delete m_negedge_event_p;
    delete m_posedge_event_p;
}

// template instantiations for writer policies

template class SC_API sc_callback_signal_t<bool, SC_ONE_WRITER>;
template class SC_API sc_callback_signal_t<bool, SC_MANY_WRITERS>;
template class SC_API sc_callback_signal_t<bool, SC_UNCHECKED_WRITERS>;

template class SC_API sc_callback_signal<bool, SC_ONE_WRITER>;
template class SC_API sc_callback_signal<bool, SC_MANY_WRITERS>;
template class SC_API sc_callback_signal<bool, SC_UNCHECKED_WRITERS>;

template class SC_API sc_callback_signal_t<sc_logic, SC_ONE_WRITER>;
template class SC_API sc_callback_signal_t<sc_logic, SC_MANY_WRITERS>;
template class SC_API sc_callback_signal_t<sc_logic, SC_UNCHECKED_WRITERS>;

template class SC_API sc_callback_signal<sc_logic, SC_ONE_WRITER>;
template class SC_API sc_callback_signal<sc_logic, SC_MANY_WRITERS>;
template class SC_API sc_callback_signal<sc_logic, SC_UNCHECKED_WRITERS>;

}  // namespace sc_core
