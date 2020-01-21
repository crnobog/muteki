#pragma once

#include "Math.h"
#include "PrimitiveTypes.h"
#include "Debug.h"
#include "RangeIteration.h"

namespace mu {
	u64 HashMemory(void*, size_t);

	template<typename T>
	u64 HashKey(const T& key) {
		return HashMemory((void*)&key, sizeof(T));
	}

	// Used because we have issues destructuring std::tuple in VS2017 so far.
	template<typename K, typename V>
	struct HashPair {
		const K& Key;
		V& Value;
	};

	struct HashState {
		enum {
			Empty = 0,
			Filled = 1,
			Removed = 2
		};

		u64 Hash : 62;
		u64 State : 2;
	};

	namespace HashUtil {
		inline size_t FirstFilled(size_t start_index, const HashState* state, size_t max) {
			for (size_t i = start_index; i < max; ++i) {
				if (state[i].State == HashState::Filled) {
					return i;
				}
			}
			return max;
		}
	}

	// Simple hashtable with open addressing by linear probing.
	// Separates hashstates, keys and values into separate arrays for fast iteration and probing.
	template<typename KeyType, typename ValueType>
	class HashTable {
	public:
		//Public interface functions forward declared

		// Add a key/value association. 
		// Overwrites the existing value if there is one.
		void Add(const KeyType& key, const ValueType& value);

		// Remove a key/value pair.
		// Uses move constructors to move the value into the return value
		ValueType Remove(const KeyType& key);
		
		// Returns true of the hashtable contains a value associated with the given key
		bool Contains(const KeyType& key) const;

		// Returns a reference to the value associated with the given key.
		ValueType& Find(const KeyType& key);
		const ValueType& Find(const KeyType& key) const;
		ValueType FindOrDefault(const KeyType& key, ValueType value = {}) const;
		ValueType& operator[](const KeyType& key) { return Find(key); }
		const ValueType& operator[](const KeyType& key) const { return Find(key); }

		// Returns the number of key/value pairs currently allocated
		size_t GetNum();

		// Make sure we have space for at least the given number of items
		void Reserve(size_t new_size);

		// Constructor/destructor and copy/move mechanisms
		HashTable();
		HashTable(const HashTable& other);
		HashTable(HashTable&& other);
		~HashTable();

		HashTable& operator=(const HashTable& other);
		HashTable& operator=(HashTable&& other);
	private:
		// A range over all the allocated key/value pairs in the hashtable
		struct HashRange {
			size_t				m_current_index;
			size_t				m_size;
			const HashState*	m_hash_state;
			const KeyType*		m_keys;
			ValueType*			m_values;

			static constexpr bool HasSize = false;

			bool IsEmpty() { return m_current_index >= m_size; }
			HashPair<const KeyType&, ValueType&> Front() { return { m_keys[m_current_index], m_values[m_current_index] }; }
			void Advance() {
				m_current_index = HashUtil::FirstFilled(m_current_index + 1, m_hash_state, m_size);
			}

			MU_RANGE_ITERATION_SUPPORT
		};

		struct ConstHashRange {
			size_t				m_current_index;
			size_t				m_size;
			const HashState*	m_hash_state;
			const KeyType*		m_keys;
			const ValueType*	m_values;

			static constexpr bool HasSize = false;

			bool IsEmpty() { return m_current_index >= m_size; }
			HashPair<const KeyType&, const ValueType&> Front() { return { m_keys[current_index],m_values[m_current_index] }; }
			void Advance() {
				m_current_index = HashUtil::FirstFilled(m_current_index + 1, m_hash_state, m_size);
			}

			MU_RANGE_ITERATION_SUPPORT
		};

		size_t		m_size = 0;
		size_t		m_used = 0;
		HashState*	m_hash_state = nullptr;
		KeyType*	m_keys = nullptr;
		ValueType*	m_values = nullptr;

		static constexpr u64 s_hash_mask = 0x3FFFFFFFFFFFFFFFULL;
		static constexpr float s_max_load = 2.0f / 3.0f;
		static constexpr size_t s_min_size = 64;

		void InitHashState() {
			for (size_t i = 0; i < m_size; ++i) {
				m_hash_state[i].State = HashState::Empty;
			}
		}
		void Deallocate() {
			delete[] m_hash_state; m_hash_state = nullptr;

			u8* old_keys = (u8*)m_keys;
			m_keys = nullptr;
			delete[] old_keys; 

			u8* old_values = (u8*)m_values;
			m_values = nullptr;
			delete[] old_values;
		}
		void Allocate() {
			m_hash_state = new HashState [m_size];
			m_keys = (KeyType*)new u8[m_size * sizeof(KeyType)];
			m_values = (ValueType*)new u8[m_size * sizeof(ValueType)];
		}
		void Grow(size_t new_size) {
			size_t old_used = m_used;
			size_t old_size = m_size;
			HashState* old_hash_state = m_hash_state;
			KeyType* old_keys = m_keys;
			ValueType* old_values = m_values;

			m_size = new_size;
			m_used = 0;
			Allocate();
			InitHashState();
			
			for (size_t i = 0; i < old_size; ++i) {
				if (old_hash_state[i].State == HashState::Filled) {
					AddHashed(std::move(old_keys[i]), old_hash_state[i].Hash, std::move(old_values[i]));
				}
			}

			Assert(m_used == old_used);
		}

		void AddHashed(const KeyType& key, u64 hash, const ValueType& value) {
			u64 index = 0;
			if (FindIndex(key, hash, index)) {
				// Overwrite existing value
				m_values[index] = value; // Should we insted destroy and create?
			}
			else {
				Assert(m_hash_state[index].State != HashState::Filled);

				new (m_keys + index) KeyType(key);
				new (m_values + index) ValueType(value); // Placement new into potentially uninitialized memory
				++m_used;
				m_hash_state[index].State = HashState::Filled;
				m_hash_state[index].Hash = hash;
			}
		}

		static u64 CalcHash(const KeyType& key)  {
			return HashKey(key) & s_hash_mask;
		}

		bool FindIndex(const KeyType& key, u64 hash, size_t& out_index) const {
			if (m_size == 0) {
				return false;
			}
			const u64 bucket = hash % m_size;
			u64 first_empty = u64_max;

			for (u64 u = 0; u < m_size; ++u) {
				u64 i = (bucket + u) % m_size;
				switch (m_hash_state[i].State) {
				case HashState::Empty: 
					out_index = Min(first_empty, i);
					return false; // If we hit empty, nothing could ever have been chained with this key. Removed doesn't guarantee that
				case HashState::Filled:
					if (m_hash_state[i].Hash == hash && m_keys[i] == key) {
						out_index = i;
						return true;
					}
					break;
				case HashState::Removed:
					first_empty = Min(first_empty, i);
					break;
				}
			}
			out_index = first_empty;
			return false;
		}

		void DestroyItems() {
			for (size_t i = 0; i < m_size; ++i) {
				if (m_hash_state[i].State == HashState::Filled) {
					m_values[i].~ValueType();
					m_keys[i].~KeyType();
				}
			}
		}
	public:

		HashRange Range() { return { HashUtil::FirstFilled(0, m_hash_state, m_size), m_size, m_hash_state, m_keys, m_values }; }
		ConstHashRange Range() const { return { HashUtil::FirstFilled(0, m_hash_state, m_size), m_size, m_hash_state, m_keys, m_values }; }
	};

	template<typename KeyType, typename ValueType>
	HashTable<KeyType, ValueType>::HashTable() {}
	
	template<typename KeyType, typename ValueType>
	HashTable<KeyType, ValueType>::HashTable(const HashTable& other) {
		*this = other;
	}
	
	template<typename KeyType, typename ValueType>
	HashTable<KeyType, ValueType>::HashTable(HashTable&& other) {
		*this = other;
	}
	
	template<typename KeyType, typename ValueType>
	HashTable<KeyType, ValueType>::~HashTable() {
		DestroyItems();
		Deallocate();
	}

	template<typename KeyType, typename ValueType>
	HashTable<KeyType, ValueType>& HashTable<KeyType, ValueType>::operator=(const HashTable& other) {
		if (m_size != other.m_size) {
			DestroyItems();
			Deallocate();
			m_size = other.m_size;
			Allocate();
		}

		memcpy(m_hash_state, other.m_hash_state, sizeof(i64) * m_size);
		m_used = other.m_used;
		for (size_t i = 0; i < m_size; ++i) {
			if (m_hash_state[i].State == HashState::Filled) {
				new(m_keys + i) KeyType(other.m_keys[i]);
				new(m_values + i) ValueType(other.m_values[i]);
			}
		}
		return *this;
	}

	template<typename KeyType, typename ValueType>
	HashTable<KeyType, ValueType>& HashTable<KeyType, ValueType>::operator=(HashTable&& other) {
		DestroyItems();
		Deallocate();

		m_size = other.m_size;
		m_used = other.m_used;
		m_hash_state = other.m_hash_state;	other.m_hash_state = nullptr;
		m_keys = other.m_keys;				other.m_keys = nullptr;
		m_values = other.m_values;			other.m_values = nullptr;
		return *this;
	}

	template<typename KeyType, typename ValueType>
	size_t HashTable<KeyType, ValueType>::GetNum() { return m_used; }

	template<typename KeyType, typename ValueType>
	void HashTable<KeyType, ValueType>::Reserve(size_t new_size) {
		if (m_size >= new_size) {
			return;
		}
		Grow(new_size);
	}

	template<typename KeyType, typename ValueType>
	void HashTable<KeyType, ValueType>::Add(const KeyType& key, const ValueType& value) {
		// Grow the hashtable if we don't have space
		if (m_used >= (m_size * s_max_load)) {
			Grow(Max(m_size * 2, s_min_size)); // TODO: Revisit growth factor. Should we always do powers of 2 and remove modulus?
		}

		u64 hash = CalcHash(key);
		AddHashed(key, hash, value);
	}

	template<typename KeyType, typename ValueType>
	ValueType HashTable<KeyType, ValueType>::Remove(const KeyType& key) {
		size_t idx = 0;
		u64 hash = CalcHash(key);
		if (FindIndex(key, hash, idx)) {
			m_hash_state[idx].State = HashState::Removed;
			m_keys[idx].~KeyType();
			--m_used;
			return std::move(m_values[idx]);
		}
		Assert(false);
		__assume(false);
	}

	template<typename KeyType, typename ValueType>
	bool HashTable<KeyType, ValueType>::Contains(const KeyType& key) const {
		size_t unused = 0;
		u64 hash = CalcHash(key);
		return FindIndex(key, hash, unused);
	}

	template<typename KeyType, typename ValueType>
	ValueType& HashTable<KeyType, ValueType>::Find(const KeyType& key) {
		return const_cast<ValueType&>(const_cast<const HashTable<KeyType, ValueType>*>(this)->Find(key));
	}

	template<typename KeyType, typename ValueType>
	const ValueType& HashTable<KeyType, ValueType>::Find(const KeyType& key) const {
		size_t idx = 0;
		u64 hash = CalcHash(key);
		if (FindIndex(key, hash, idx)) {
			return m_values[idx];
		}
		Assert(false);
		__assume(false);
	}

	template<typename KeyType, typename ValueType>
	ValueType HashTable<KeyType, ValueType>::FindOrDefault(const KeyType& key, ValueType value) const {
		size_t idx = 0;
		u64 hash = CalcHash(key);
		if (FindIndex(key, hash, idx)) {
			return m_values[idx];
		}
		return std::move(value);
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/HashTable_Tests.inl"
#endif

#ifdef BENCHMARK
#include "Benchmarks/HashTable_Benchmarks.inl"
#endif