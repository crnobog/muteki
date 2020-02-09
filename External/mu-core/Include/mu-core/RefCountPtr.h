#pragma once

// Smart pointer for managing objects with internal reference counting such as COM objects
// POLICY should be a template accepting OBJECT which implements static functions IncRef and DecRef
template<typename OBJECT, template<typename> class POLICY>
class RefCountPtr
{
	OBJECT* m_ptr;

	void IncRef() {
		POLICY<OBJECT>::IncRef(m_ptr);
	}

	void DecRef() {
		POLICY<OBJECT>::DecRef(m_ptr);
	}

public:

	RefCountPtr() : m_ptr(nullptr) {
	}
	RefCountPtr(nullptr_t) : m_ptr(nullptr) {
	}

	RefCountPtr(const RefCountPtr& other) : m_ptr(other.Get()) {
		IncRef();
	}

	RefCountPtr(RefCountPtr&& other) : m_ptr(other.Steal()) {
	}

	~RefCountPtr() {
		if (m_ptr) {
			DecRef();
			m_ptr = nullptr;
		}
	}

	RefCountPtr& operator=(nullptr_t) {
		Clear();
		return *this;
	}

	RefCountPtr& operator=(const RefCountPtr& other) {
		Clear();
		m_ptr = other.m_ptr;
		IncRef();
		return *this;
	}

	RefCountPtr& operator=(RefCountPtr&& other) {
		Clear();
		m_ptr = other.Steal();
		return *this;
	}

	OBJECT* Steal() {
		OBJECT* ptr = m_ptr;
		m_ptr = nullptr;
		return ptr;
	}

	void Set(OBJECT* ptr) {
		Clear();
		m_ptr = ptr;
		IncRef();
	}

	void Clear() {
		if (m_ptr) {
			DecRef();
			m_ptr = nullptr;
		}
	}

	OBJECT** Replace() {
		Clear();
		return &m_ptr;
	}

	OBJECT* Get() const { return m_ptr; }
	OBJECT* operator->() const { return m_ptr; }
	operator OBJECT*() const { return m_ptr; }

	explicit operator bool() const {
		return m_ptr != nullptr;
	}
	bool operator!() const {
		return m_ptr == nullptr;
	}
};