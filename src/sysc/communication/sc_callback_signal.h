#ifndef SC_CALLBACK_SIGNAL_H
#define SC_CALLBACK_SIGNAL_H

#include "sysc/communication/sc_signal.h"

#include <functional>

namespace sc_core {

template <class T, sc_writer_policy POL>
class sc_callback_signal_t : public sc_signal_inout_if<T>,
                             public sc_signal_channel,
                             protected sc_writer_policy_check<POL> {
  protected:
    typedef sc_signal_inout_if<T> if_type;
    typedef sc_signal_channel base_type;
    typedef sc_callback_signal_t<T, POL> this_type;
    typedef sc_writer_policy_check<POL> policy_type;

  protected:
    // constructor and destructor

    sc_callback_signal_t(const char *name_, const T &initial_value_)
        : base_type(name_), m_cur_val(initial_value_), m_new_val(initial_value_) {}

    virtual ~sc_callback_signal_t() {} /* = default; */

  public:
    // interface methods

    virtual const char *kind() const { return "sc_signal"; }

    virtual void register_port(sc_port_base &, const char *);

    virtual sc_writer_policy get_writer_policy() const { return POL; }

    // get the default event
    virtual const sc_event &default_event() const { return value_changed_event(); }

    // get the value changed event
    virtual const sc_event &value_changed_event() const { return base_type::value_changed_event(); }

    // read the current value
    virtual const T &read() const { return m_cur_val; }

    // get a reference to the current value (for tracing)
    virtual const T &get_data_ref() const {
        deprecated_get_data_ref();
        return m_cur_val;
    }

    // was there an event?
    virtual bool event() const { return base_type::event(); }

    // write the new value
    virtual void write(const T &);

    // other methods

    operator const T &() const { return read(); }

    // assignment
    this_type &operator=(const T &a) {
        write(a);
        return *this;
    }

    this_type &operator=(const sc_signal_in_if<T> &a) {
        write(a.read());
        return *this;
    }

    this_type &operator=(const this_type &a) {
        write(a.read());
        return *this;
    }

    const T &get_new_value() const {
        deprecated_get_new_value();
        return m_new_val;
    }

    void trace(sc_trace_file *tf) const {
        deprecated_trace();
#ifdef DEBUG_SYSTEMC
        sc_trace(tf, read(), name());
#else
        if (tf) {
        }
#endif
    }

    virtual void print(::std::ostream & = ::std::cout) const;
    virtual void dump(::std::ostream & = ::std::cout) const;
    virtual void set_callback(const std::function<void(const T&, void*)> &callback,
                              void *param);

  protected:
    virtual void update();
    void do_update();

  protected:
    T m_cur_val;  // current value of object.
    T m_new_val;  // next value of object.
    std::function<void(const T&, void*)> m_callback;
    void* m_callback_param;

  private:
    // disabled
    sc_callback_signal_t(const sc_callback_signal_t &) /* = delete */;
};

// ----------------------------------------------------------------------------

template <class T, sc_writer_policy POL>
inline void sc_callback_signal_t<T, POL>::register_port(sc_port_base &port_,
                                                        const char *if_typename_) {
    bool is_output = std::string(if_typename_) == typeid(if_type).name();
    if (!policy_type::check_port(this, &port_, is_output))
        ((void)0);  // fallback? error has been suppressed ...
}

// write the new value

template <class T, sc_writer_policy POL>
inline void sc_callback_signal_t<T, POL>::write(const T &value_) {
    // first write per eval phase: m_new_val == m_cur_val
    bool value_changed = !(m_new_val == value_);
    if (!policy_type::check_write(this, value_changed))
        return;

    if (value_changed && m_callback) {
        m_callback(value_, m_callback_param);
    }

    m_new_val = value_;
    if (value_changed || policy_type::needs_update()) {
        request_update();
    }
}

template <class T, sc_writer_policy POL>
inline void sc_callback_signal_t<T, POL>::print(::std::ostream &os) const {
    os << m_cur_val;
}

template <class T, sc_writer_policy POL>
void sc_callback_signal_t<T, POL>::dump(::std::ostream &os) const {
    os << "     name = " << name() << ::std::endl;
    os << "    value = " << m_cur_val << ::std::endl;
    os << "new value = " << m_new_val << ::std::endl;
}

template <class T, sc_writer_policy POL>
void sc_callback_signal_t<T, POL>::set_callback(
        const std::function<void(const T&, void*)> &callback, void *param) {
    m_callback = callback;
    m_callback_param = param;
}

template <class T, sc_writer_policy POL>
void sc_callback_signal_t<T, POL>::update() {
    policy_type::update();
    if (!(m_new_val == m_cur_val)) {
        do_update();
    }
}

template <class T, sc_writer_policy POL>
inline void sc_callback_signal_t<T, POL>::do_update() {
    base_type::do_update();
    m_cur_val = m_new_val;
}

// ----------------------------------------------------------------------------
//  CLASS : sc_signal<T, POL>
//
//  The sc_signal<T,POL> primitive channel class
// ----------------------------------------------------------------------------

template <class T, sc_writer_policy POL /* = SC_DEFAULT_WRITER_POLICY */>
class sc_callback_signal : public sc_callback_signal_t<T, POL> {
  public:
    typedef sc_signal_inout_if<T> if_type;
    typedef sc_callback_signal_t<T, POL> base_type;
    typedef sc_callback_signal<T, POL> this_type;
    typedef T value_type;
    typedef sc_writer_policy_check<POL> policy_type;

    // constructors and destructor

    sc_callback_signal() : base_type(sc_gen_unique_name("signal"), value_type()) {}

    explicit sc_callback_signal(const char *name_) : base_type(name_, value_type()) {}

    sc_callback_signal(const char *name_, const value_type &initial_value_)
        : base_type(name_, initial_value_) {}

    virtual ~sc_callback_signal() {}

    // assignment
    this_type &operator=(const value_type &a) {
        base_type::operator=(a);
        return *this;
    }

    this_type &operator=(const sc_signal_in_if<value_type> &a) {
        base_type::operator=(a);
        return *this;
    }

    this_type &operator=(const this_type &a) {
        base_type::operator=(a);
        return *this;
    }

  private:
    // disabled
    sc_callback_signal(const sc_callback_signal &) /* = delete */;
};

SC_API_TEMPLATE_DECL_ sc_callback_signal_t<bool, SC_ONE_WRITER>;
SC_API_TEMPLATE_DECL_ sc_callback_signal_t<bool, SC_MANY_WRITERS>;
SC_API_TEMPLATE_DECL_ sc_callback_signal_t<bool, SC_UNCHECKED_WRITERS>;

template <sc_writer_policy POL>
class SC_API sc_callback_signal<bool, POL> : public sc_callback_signal_t<bool, POL> {
  protected:
    typedef sc_callback_signal_t<bool, POL> base_type;
    typedef sc_callback_signal<bool, POL> this_type;
    typedef bool value_type;
    typedef sc_writer_policy_check<POL> policy_type;

  public:
    // constructors and destructor

    sc_callback_signal()
        : base_type(sc_gen_unique_name("signal"), value_type()),
          m_negedge_event_p(0),
          m_posedge_event_p(0),
          m_reset_p(0) {}

    explicit sc_callback_signal(const char *name_)
        : base_type(name_, value_type()),
          m_negedge_event_p(0),
          m_posedge_event_p(0),
          m_reset_p(0) {}

    sc_callback_signal(const char *name_, const value_type &initial_value_)
        : base_type(name_, initial_value_),
          m_negedge_event_p(0),
          m_posedge_event_p(0),
          m_reset_p(0) {}

    virtual ~sc_callback_signal();

    // get the positive edge event
    virtual const sc_event &posedge_event() const;

    // get the negative edge event
    virtual const sc_event &negedge_event() const;

    // was there a positive edge event?
    virtual bool posedge() const { return (this->event() && this->m_cur_val); }

    // was there a negative edge event?
    virtual bool negedge() const { return (this->event() && !this->m_cur_val); }

    // assignment
    this_type &operator=(const value_type &a) {
        base_type::operator=(a);
        return *this;
    }

    this_type &operator=(const sc_signal_in_if<value_type> &a) {
        base_type::operator=(a);
        return *this;
    }

    this_type &operator=(const this_type &a) {
        base_type::operator=(a);
        return *this;
    }

  protected:
    virtual void update();
    void do_update();

    virtual bool is_clock() const { return false; }

  protected:
    mutable sc_event *m_negedge_event_p;  // negative edge event if present.
    mutable sc_event *m_posedge_event_p;  // positive edge event if present.
    mutable sc_reset *m_reset_p;          // reset mechanism if present.

  private:
    // reset creation
    virtual sc_reset *is_reset() const;

    // disabled
    sc_callback_signal(const this_type &) /* = delete */;
};

// ----------------------------------------------------------------------------
//  CLASS : sc_signal<sc_dt::sc_logic>
//
//  Specialization of sc_signal<T> for type sc_dt::sc_logic.
// ----------------------------------------------------------------------------

SC_API_TEMPLATE_DECL_ sc_callback_signal_t<sc_dt::sc_logic, SC_ONE_WRITER>;
SC_API_TEMPLATE_DECL_ sc_callback_signal_t<sc_dt::sc_logic, SC_MANY_WRITERS>;
SC_API_TEMPLATE_DECL_ sc_callback_signal_t<sc_dt::sc_logic, SC_UNCHECKED_WRITERS>;

template <sc_writer_policy POL>
class SC_API sc_callback_signal<sc_dt::sc_logic, POL>
    : public sc_callback_signal_t<sc_dt::sc_logic, POL> {
  protected:
    typedef sc_callback_signal_t<sc_dt::sc_logic, POL> base_type;
    typedef sc_callback_signal<sc_dt::sc_logic, POL> this_type;
    typedef sc_dt::sc_logic value_type;
    typedef sc_writer_policy_check<POL> policy_type;

  public:
    sc_callback_signal()
        : base_type(sc_gen_unique_name("signal"), value_type()),
          m_negedge_event_p(0),
          m_posedge_event_p(0) {}

    explicit sc_callback_signal(const char *name_)
        : base_type(name_, value_type()), m_negedge_event_p(0), m_posedge_event_p(0) {}

    sc_callback_signal(const char *name_, const value_type &initial_value_)
        : base_type(name_, initial_value_), m_negedge_event_p(0), m_posedge_event_p(0) {}

    virtual ~sc_callback_signal();

    // get the positive edge event
    virtual const sc_event &posedge_event() const;

    // get the negative edge event
    virtual const sc_event &negedge_event() const;

    // was there a positive edge event?
    virtual bool posedge() const { return (this->event() && this->m_cur_val == sc_dt::SC_LOGIC_1); }

    // was there a negative edge event?
    virtual bool negedge() const { return (this->event() && this->m_cur_val == sc_dt::SC_LOGIC_0); }

    // assignment
    this_type &operator=(const value_type &a) {
        base_type::operator=(a);
        return *this;
    }

    this_type &operator=(const sc_signal_in_if<value_type> &a) {
        base_type::operator=(a);
        return *this;
    }

    this_type &operator=(const this_type &a) {
        base_type::operator=(a);
        return *this;
    }

  protected:
    virtual void update();
    void do_update();

  protected:
    mutable sc_event *m_negedge_event_p;  // negative edge event if present.
    mutable sc_event *m_posedge_event_p;  // positive edge event if present.

  private:
    // disabled
    sc_callback_signal(const this_type &) /* = delete */;
};

}  // namespace sc_core

#endif
