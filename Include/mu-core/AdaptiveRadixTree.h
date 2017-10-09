#pragma once

#include "Array.h"
#include "Debug.h"
#include "PrimitiveTypes.h"
#include "PointerRange.h"
#include "TaggedPointer.h"
#include "Math.h"

// TODO: Consider case where a key is completely a prefix of an existing key.
//	Does NodeBase need to have space for a leaf? 
//	Can we enforce no key is a prefix of another e.g. string keys will be null terminated with no inner null bytes
namespace mu {
	// Associative lookup of values from arbitrary length keys
	// Stored values may not be null pointers
	class AdaptiveRadixTree {
		struct ChildBase;
		struct NodeBase;
		struct Node4;
		struct Node16;
		struct Node48;
		struct Node256;
		struct Leaf;

		//typedef TaggedPointer<void*, bool, 1> ChildPtr;
		struct ChildPtr : public TaggedPointer<void*, bool, 1> {
			ChildPtr() : TaggedPointer(nullptr, 0) {}
			ChildPtr(nullptr_t) : TaggedPointer(nullptr, 0) {}
			ChildPtr(NodeBase* node) : TaggedPointer(node, 0) {}
			ChildPtr(Leaf* leaf) : TaggedPointer(leaf, 1) {}

			bool operator!=(nullptr_t) { return GetRaw() != 0; }
			bool operator==(nullptr_t) { return GetRaw() == 0; }
		};

		static constexpr i32 MAX_INLINE_PREFIX = 6;
		struct NodeBase {
			u8 m_num_children;
			u8 m_type;
			u8 m_partial_prefix[MAX_INLINE_PREFIX];
			i32 m_prefix_len;

			NodeBase(NodeBase* old, u8 type)
				: m_num_children(0)
				, m_prefix_len(0)
				, m_type(type) {
				if (old) {
					m_num_children = old->m_num_children;
					m_prefix_len = old->m_prefix_len;
					memcpy(m_partial_prefix, old->m_partial_prefix, MAX_INLINE_PREFIX * sizeof(u8));
				}
			}
		};
		struct Node4 : public NodeBase {
			u8 m_keys[4];
			ChildPtr m_children[4];

			Node4(NodeBase* old) : NodeBase(old, TYPE) {
				// No initialization of children, m_num_children makes it unnecessary
			}

			static constexpr u8 TYPE = 0;
		};
		struct Node16 : public NodeBase {
			u8 m_keys[16];
			ChildPtr m_children[16];

			Node16(NodeBase* old) : NodeBase(old, TYPE) {
				// No initialization of children, m_num_children makes it unnecessary
			}

			static constexpr u8 TYPE = 1;
		};
		struct Node48 : public NodeBase {
			u8 m_key_to_index[256];
			ChildPtr m_children[48];

			Node48(NodeBase* old) : NodeBase(old, TYPE) {
				memset(m_key_to_index, 0xFF, sizeof(m_key_to_index));
				memset(m_children, 0, sizeof(m_children));
			}

			static constexpr u8 TYPE = 2;
		};
		struct Node256 : public NodeBase {
			ChildPtr m_children[256];

			Node256(NodeBase* old) : NodeBase(old, TYPE) {
				memset(m_children, 0, sizeof(m_children));
			}

			static constexpr u8 TYPE = 3;
		};

		struct Leaf {
			void* m_value;
			Array<u8> m_key;

			Leaf(PointerRange<const u8> key, void* value)
				: m_value(value) {
				m_key.Append(key);
			}
		};

		struct FindResult {
			enum {
				ExactMatch,		// Found a leaf matching the key
				LeafNonMatch,	// Search terminated at a leaf that didn't match the key
				NotFound,		// Search terminated at a node with no child matching the critical byte
				PrefixMismatch,	// Search terminated at a node whose prefix didn't match the key
			} m_result;

			ChildPtr* m_node;
			ChildPtr* m_parent;
			i32 m_depth;
			i32 m_match_len;
			PointerRange<const u8> m_leaf_key;
		};

		ChildPtr m_root;

	public:
		AdaptiveRadixTree() {}
		~AdaptiveRadixTree() {}

		void* Add(PointerRange<const u8> key, void* value) {
			Assert(key.Size() > 0);
			Assert(value != nullptr);

			if (m_root == nullptr) {
				// Insert a leaf
				m_root = { new Leaf(key, value) };
				return nullptr;
			}

			//return AddRecursive(m_root, &m_root, key, 0, value);
			auto result = FindInternal(m_root, &m_root, nullptr, key, 0);
			switch (result.m_result) {
			case FindResult::ExactMatch:
			{
				Leaf* leaf = (Leaf*)result.m_node->GetPointer();
				void* old_value = leaf->m_value;
				leaf->m_value = value;
				return old_value;
			}
			case FindResult::LeafNonMatch:
			{
				// Replace leaf with a node4 with both leaves as children
				Node4* new_node = new Node4(nullptr);

				Leaf* new_leaf = new Leaf(key, value);
				Leaf* old_leaf = (Leaf*)result.m_node->GetPointer();

				// Find common prefix
				new_node->m_prefix_len = result.m_match_len;
				memcpy(new_node->m_partial_prefix, &key[result.m_depth], sizeof(u8) * Min(MAX_INLINE_PREFIX, result.m_match_len));

				i32 crit_byte = result.m_depth + result.m_match_len;
				AddChild4(new_node, result.m_parent, key[crit_byte], { new_leaf });
				AddChild4(new_node, result.m_parent, old_leaf->m_key[crit_byte], { old_leaf });

				*result.m_node = { new_node };
				return nullptr;
			}
			case FindResult::PrefixMismatch:
			{
				// Make a new node with a new leaf and this node as children
				NodeBase* old_node = (NodeBase*)result.m_node->GetPointer();
				Node4* new_node = new Node4(nullptr);
				new_node->m_prefix_len = result.m_match_len;
				memcpy(new_node->m_partial_prefix, old_node->m_partial_prefix, Min(MAX_INLINE_PREFIX, result.m_match_len));
				*result.m_node = { new_node };

				if (old_node->m_prefix_len <= MAX_INLINE_PREFIX) {
					// We stored all the prefix we can, just need to shorten it by the match length
					AddChild4(new_node, result.m_parent, old_node->m_partial_prefix[result.m_match_len], { old_node });
					// +1 in both of these to skip the byte we index by in AddChild above
					old_node->m_prefix_len -= result.m_match_len + 1;
					memmove(old_node->m_partial_prefix,
						old_node->m_partial_prefix + result.m_match_len + 1,
						Min(MAX_INLINE_PREFIX, old_node->m_prefix_len)
					);
				}
				else {
					// We need to pull in as much key as we can from the leaf
					old_node->m_prefix_len -= result.m_match_len + 1;
					AddChild4(new_node, result.m_parent, result.m_leaf_key[result.m_depth + result.m_match_len], { old_node });
					memcpy(new_node->m_partial_prefix,
						&result.m_leaf_key[result.m_depth + result.m_match_len + 1],
						Min(MAX_INLINE_PREFIX, old_node->m_prefix_len)
					);
				}

				Leaf* new_leaf = new Leaf(key, value);
				AddChild4(new_node, result.m_parent, key[result.m_depth + result.m_match_len], { new_leaf });
				return nullptr;
			}
			case FindResult::NotFound:
			{
				// Add a new leaf under this node
				Leaf* new_leaf = new Leaf(key, value);
				AddChild((NodeBase*)result.m_node->GetPointer(), result.m_node, key[result.m_depth], { new_leaf });
				return nullptr;
			}
			}
			__assume(false);
		}
		void* Remove(PointerRange<const u8> key) {
			Assert(key.Size() > 0);

			if (m_root == nullptr) { return nullptr; }

			auto result = FindInternal(m_root, &m_root, nullptr, key, 0);
			if (result.m_result == FindResult::ExactMatch) {
				Leaf* leaf = (Leaf*)result.m_node->GetPointer();
				void* value = leaf->m_value;
				if (result.m_depth > 0) {
					RemoveChild((NodeBase*)result.m_parent->GetPointer(), result.m_parent, key[result.m_depth - 1]);
				}
				else {
					Assert(result.m_node == &m_root);
					m_root = { nullptr };
				}
				delete leaf;
				return value;
			}
			return nullptr;
		}
		void* Find(PointerRange<const u8> key) {
			Assert(key.Size() > 0);

			if (m_root == nullptr) { return nullptr; }
			auto result = FindInternal(m_root, &m_root, nullptr, key, 0);
			if (result.m_result == FindResult::ExactMatch) {
				Leaf* l = (Leaf*)result.m_node->GetPointer();
				return l->m_value;
			}
			return nullptr;
		}

	private:
		Leaf* MinimumNode(NodeBase* n) {
			ChildPtr child{ n };
			while (child != nullptr) {
				if (child.GetInt() == 1) {
					return (Leaf*)child.GetPointer();
				}

				NodeBase* node = (NodeBase*)child.GetPointer();
				switch (node->m_type) {
				case Node4::TYPE:	child = ((Node4*)node)->m_children[0]; break;
				case Node16::TYPE:	child = ((Node16*)node)->m_children[0]; break;
				case Node48::TYPE:
				{
					Node48* n48 = (Node48*)node;
					for (i32 i = 0; i < 256; ++i) {
						if (n48->m_key_to_index[i] != 0xFF) {
							child = n48->m_children[n48->m_key_to_index[i]];
							break;
						}
					}
					continue;
				}
				case Node256::TYPE:
				{
					Node256* n256 = (Node256*)node;
					for (i32 i = 0; i < 256; ++i) {
						if (n256->m_children[i] != nullptr) {
							child = n256->m_children[i];
							break;
						}
					}
					continue;
				}
				}
			}
			__assume(false);
		}

		bool KeyMatches(Leaf* leaf, PointerRange<const u8> key) {
			if (leaf->m_key.Num() == key.Size()) {
				for (i32 i = 0; !key.IsEmpty(); ++i, key.Advance()) {
					if (leaf->m_key[i] != key.Front()) { return false; };
				}
				return true;
			}
			return false;
		}

		i32 PrefixMatch(NodeBase* n, PointerRange<const u8> key, i32 depth, PointerRange<const u8>& out_leaf_key) {
			i32 match_len = 0, max_cmp = Min((i32)key.Size(), Min(MAX_INLINE_PREFIX, n->m_prefix_len));
			for (; match_len < max_cmp; ++match_len) {
				if (key[depth + match_len] != n->m_partial_prefix[match_len]) {
					return match_len;
				}
			}

			if (n->m_prefix_len > MAX_INLINE_PREFIX) {
				Leaf* leaf = MinimumNode(n);
				out_leaf_key = Range(leaf->m_key);
				max_cmp = (i32)Min(key.Size(), leaf->m_key.Num()) - depth;
				for (; match_len < max_cmp; ++match_len) {
					if (key[depth + match_len] != leaf->m_key[depth + match_len]) {
						return match_len;
					}
				}
			}
			return match_len;
		}

		inline FindResult FindInternal(ChildPtr node_or_leaf, ChildPtr* ref, ChildPtr* parent, PointerRange<const u8> key, i32 depth) {
			Assert(node_or_leaf != nullptr);
			while (true) {
				if (node_or_leaf.GetInt() == 1) {
					Leaf* leaf = (Leaf*)node_or_leaf.GetPointer();
					i32 match_len = 0;
					for (; match_len + depth < Min(key.Size(), leaf->m_key.Num()); ++match_len) {
						if (leaf->m_key[depth + match_len] != key[depth + match_len]) {
							break;
						}
					}

					if (leaf->m_key.Num() == key.Size() && (match_len + depth) == key.Size()) {
						return FindResult{ FindResult::ExactMatch, ref, parent, depth, match_len, {} };
					}

					return FindResult{ FindResult::LeafNonMatch, ref, parent, depth, match_len, {} };
				}

				NodeBase* n = (NodeBase*)node_or_leaf.GetPointer();
				PointerRange<const u8> leaf_key;
				if (i32 match_len = PrefixMatch(n, key, depth, leaf_key); match_len < n->m_prefix_len) {
					return FindResult{ FindResult::PrefixMismatch, ref, parent, depth, match_len, leaf_key };
				}
				depth += n->m_prefix_len;

				ChildPtr* child = FindChild(n, key[depth]);
				if (child) {
					parent = ref;
					node_or_leaf = *child;
					ref = child;
					++depth;
					continue;
				}
				else {
					return FindResult{ FindResult::NotFound, ref, parent, depth, {} };
				}
			}
		}

		ChildPtr* FindChild(NodeBase* n, u8 key) {
			switch (n->m_type) {
			case Node4::TYPE:	return FindChild4((Node4*)n, key);
			case Node16::TYPE:	return FindChild16((Node16*)n, key);
			case Node48::TYPE:	return FindChild48((Node48*)n, key);
			case Node256::TYPE: return FindChild256((Node256*)n, key);
			}
			return nullptr;
		}

		ChildPtr* FindChild4(Node4* n, u8 key) {
			for (i32 i = 0; i < n->m_num_children; ++i) {
				if (n->m_keys[i] == key) { return &n->m_children[i]; }
				else if (n->m_keys[i] > key) { break; }
			}
			return nullptr;
		}
		ChildPtr* FindChild16(Node16* n, u8 key) {
			for (i32 i = 0; i < n->m_num_children; ++i) {
				if (n->m_keys[i] == key) { return &n->m_children[i]; }
				else if (n->m_keys[i] > key) { break; }
			}
			return nullptr;
		}
		ChildPtr* FindChild48(Node48* n, u8 key) {
			if (n->m_key_to_index[key] == 0xFF) {
				return nullptr;
			}
			return &n->m_children[n->m_key_to_index[key]];
		}
		ChildPtr* FindChild256(Node256* n, u8 key) {
			if (n->m_children[key] == nullptr) {
				return nullptr;
			}
			return &n->m_children[key];
		}

		void AddChild(NodeBase* n, ChildPtr* ref, u8 key, ChildPtr new_child) {
			switch (n->m_type) {
			case Node4::TYPE:	return AddChild4((Node4*)n, ref, key, new_child);
			case Node16::TYPE:	return AddChild16((Node16*)n, ref, key, new_child);
			case Node48::TYPE:	return AddChild48((Node48*)n, ref, key, new_child);
			case Node256::TYPE: return AddChild256((Node256*)n, ref, key, new_child);
			}
		}

		void Insert(u8 new_key, ChildPtr new_child, u8 num_children, u8* keys, ChildPtr* children) {
			i32 i = 0;
			for (; i < num_children; ++i) {
				if (keys[i] > new_key) {
					memmove(keys + i + 1, keys + i, num_children - i);
					memmove(children + i + 1, children + i, num_children - i);
					break;
				}
			}
			keys[i] = new_key;
			children[i] = new_child;
			return;
		}

		void AddChild4(Node4* n, ChildPtr* ref, u8 key, ChildPtr new_child) {
			if (n->m_num_children == 4) {
				// Replace with a Node16
				Node16* new_node = new Node16(n);
				*ref = { new_node };

				memcpy(new_node->m_keys, n->m_keys, n->m_num_children * sizeof(u8));
				memcpy(new_node->m_children, n->m_children, n->m_num_children * sizeof(ChildPtr));
				delete n;

				AddChild16(new_node, ref, key, new_child);
			}
			else {
				Insert(key, new_child, n->m_num_children, n->m_keys, n->m_children);
				++n->m_num_children;
			}
		}
		void AddChild16(Node16* n, ChildPtr* ref, u8 key, ChildPtr new_child) {
			if (n->m_num_children == 16) {
				// Replace with Node48
				Node48* new_node = new Node48(n);
				*ref = { new_node };

				for (u8 i = 0; i < n->m_num_children; ++i) {
					new_node->m_key_to_index[n->m_keys[i]] = i;
					new_node->m_children[i] = n->m_children[i];
				}

				delete n;
				AddChild48(new_node, ref, key, new_child);
			}
			else {
				Insert(key, new_child, n->m_num_children, n->m_keys, n->m_children);
				++n->m_num_children;
			}
		}
		void AddChild48(Node48* n, ChildPtr* ref, u8 key, ChildPtr new_child) {
			if (n->m_num_children == 48) {
				// Replace with Node256
				Node256* new_node = new Node256(n);

				for (i32 i = 0; i < 256; ++i) {
					if (n->m_key_to_index[i] != 0xFF) {
						new_node->m_children[i] = n->m_children[n->m_key_to_index[i]];
					}
				}

				delete n;
				AddChild256(new_node, ref, key, new_child);
			}
			else {
				for (u8 i = 0; i < 48; ++i) {
					if (n->m_children[i] == nullptr) {
						n->m_key_to_index[key] = i;
						break;
					}
				}
				++n->m_num_children;
			}
		}
		void AddChild256(Node256* n, ChildPtr* /*ref*/, u8 key, ChildPtr new_child) {
			n->m_children[key] = new_child;
			++n->m_num_children;
		}

		void RemoveChild(NodeBase* n, ChildPtr* ref, u8 key) {
			switch (n->m_type) {
			case Node4::TYPE:	return RemoveChild4((Node4*)n, ref, key);
			case Node16::TYPE:	return RemoveChild16((Node16*)n, ref, key);
			case Node48::TYPE:	return RemoveChild48((Node48*)n, ref, key);
			case Node256::TYPE:	return RemoveChild256((Node256*)n, ref, key);
			}
		}

		void Remove(u8 key, u8 num_children, u8* keys, ChildPtr* children) {
			// No need to check for removing last child, just decrementing num_children is sufficient
			for (i32 i = 0; i < num_children - 1; ++i) {
				if (keys[i] == key) {
					i32 move_num = num_children - i;
					memmove(keys + i, keys + i + 1, move_num * sizeof(u8));
					memmove(children + i, children + i + 1, move_num * sizeof(ChildPtr));
					return;
				}
			}
		}
		void RemoveChild4(Node4* n, ChildPtr* ref, u8 key) {
			Remove(key, n->m_num_children, n->m_keys, n->m_children);
			--n->m_num_children;

			if (n->m_num_children == 1) {
				// Replace with our only child
				*ref = n->m_children[0];
				if (ref->GetInt() == 0) {
					NodeBase* child = (NodeBase*)ref->GetPointer();
					i32 prefix = n->m_prefix_len;

					// Full prefix is n's prefix + key + child's prefix, concat as much as possible onto n 
					// before copying it to child
					if (prefix < MAX_INLINE_PREFIX) {
						n->m_partial_prefix[prefix++] = n->m_keys[0];
					}
					if (prefix < MAX_INLINE_PREFIX) {
						i32 copy_amt = Min(child->m_prefix_len, MAX_INLINE_PREFIX - prefix);
						memcpy(n->m_partial_prefix + prefix, child->m_partial_prefix, copy_amt);
						prefix += copy_amt;
					}

					memcpy(child->m_partial_prefix, n->m_partial_prefix, Min(prefix, MAX_INLINE_PREFIX));
					child->m_prefix_len += n->m_prefix_len + 1; // 1 for key
				}

				delete n;
			}
		}
		void RemoveChild16(Node16* n, ChildPtr* ref, u8 key) {
			Remove(key, n->m_num_children, n->m_keys, n->m_children);
			--n->m_num_children;

			if (--n->m_num_children == 4) {
				// When dropping to 3, replace with a Node4
				Node4* new_node = new Node4(n);
				memcpy(new_node->m_keys, n->m_keys, n->m_num_children * sizeof(u8));
				memcpy(new_node->m_children, n->m_children, n->m_num_children * sizeof(ChildPtr));

				*ref = { new_node };
				delete n;
			}
		}
		void RemoveChild48(Node48* n, ChildPtr* ref, u8 key) {
			n->m_children[n->m_key_to_index[key]] = { nullptr };

			if (--n->m_num_children == 15) {
				// When dropping to 15, replace with a Node16
				Node16* new_node = new Node16(n);
				*ref = { new_node };
				u8 next = 0;
				for (i32 i = 0; i < 256; ++i) {
					if (u8 child = n->m_key_to_index[i]; child != 0xFF) {
						new_node->m_keys[next] = (u8)i;
						new_node->m_children[next] = n->m_children[child];
						++next;
					}
				}
				delete n;
			}
		}
		void RemoveChild256(Node256* n, ChildPtr* ref, u8 key) {
			n->m_children[key] = { nullptr };
			if (--n->m_num_children == 47) {
				// When dropping to 47, replace with a Node48
				Node48* new_node = new Node48(n);
				*ref = { new_node };

				u8 next = 0;
				for (i32 i = 0; i < 256; ++i) {
					if (n->m_children[i] != nullptr) {
						new_node->m_key_to_index[i] = next;
						new_node->m_children[next] = n->m_children[i];
						++next;
					}
				}

				delete n;
			}
		}
	};
}


#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/AdaptiveRadixTree_Tests.inl"
#endif

#ifdef BENCHMARK
#include "Benchmarks/AdaptiveRadixTree_Benchmarks.inl"
#endif
