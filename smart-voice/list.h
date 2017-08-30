/*
 * C++ program to implement Custom Linked List and
 * iterator pattern.
 */

namespace LOCAL {

template <typename T>
class list
{
	/* Forward declaration */
	class Node;

	public:
	list<T>() noexcept {
		m_pRoot = nullptr;
	}

	/*
	* Forward declaration must be done
	* in the same access scope
	*/
	class iterator;

	/* Root of list wrapped in iterator type */
	iterator begin(void) { return iterator(m_pRoot); }

	/* End of LInkedList wrapped in iterator type */
	iterator end(void)   { return iterator(nullptr); }

	/* Adds data to the end of list */
	void push_back(T data) {
		Node *pNew = GetNode(data);
		Node *pNode = GetRoot();

		if (!pNode) {
			GetRoot() = pNew;
		} else {
			while (pNode->pNext)
				pNode = pNode->pNext;

			pNode->pNext = pNew;
			pNew->pPrev = pNode;
		}
		m_Size++;
	}

	/* Returns a reference to the first element in the list container */
	T front(void) {
		Node *pNode = GetRoot();

		return pNode->Data;
	}

	/* Removes from the list container */
	iterator erase(iterator pos) {
		Node *pNode = const_cast<Node *>(pos.m_pNode);
		Node *pFind = GetRoot();

		while (pFind != pNode)
			pFind = pFind->pNext;
		
		if (!pFind)
			return NULL;

		Node *pPrev = pFind->pPrev, *pNext = pFind->pNext;

		if (pPrev)
			pPrev->pNext = pNext;

		if (pNext)
			pNext->pPrev = pPrev;

		if (pFind == GetRoot())
			GetRoot() = pNext;

		m_Size--;

		delete pFind;

		return iterator(pNext);
	}

	/* Returns the number of elements in the list container. */
	int size(void) const { return m_Size; }

	/*
	 * iterator class can be used to
	 * sequentially access nodes of linked list
	 */
	class iterator {
		public:
		iterator() { }
		iterator(const Node *pNode) noexcept :
			m_pNode (pNode) { }

		iterator & operator=(Node *pNode) {
			this->m_pNode = pNode;
			return *this;
		}

		/* Prefix ++ overload */
		iterator & operator++(void) {
			if (m_pNode)
				m_pNode = m_pNode->pNext;
			return *this;
		}

		/* Postfix ++ overload */
		iterator operator++(int) {
			iterator li = *this;
			++*this;
			return li;
		}

		/* Prefix == overload */
		bool operator==(const iterator& li) {
			return m_pNode == li.m_pNode;
		}

		/* Prefix != overload */
		bool operator!=(const iterator& li) {
			return m_pNode != li.m_pNode;
		}

		T operator *(void) { return m_pNode->Data; }

		const Node *m_pNode = NULL;
	};

	private:
	class Node {
		T Data;
		Node *pNext, *pPrev;

		/*
		 * list class methods need
		 * to access Node information
		 */
		friend class list;
	};

	/* Create a new Node */
	Node *GetNode(T data) {
		Node *pNode = new Node;

		pNode->Data = data;
		pNode->pNext = nullptr;
		pNode->pPrev = nullptr;
		return pNode;
	}

	/*
	 * Return by reference so that it can be used in
	 * left hand side of the assignment expression
	 */
	Node *&GetRoot(void) { return m_pRoot; }

	Node *m_pRoot = NULL;
	int m_Size = 0;
};

template<class T, class Type> T find(T first, T last, const Type val)
{
	while (first != last && !(*first == val))
		++first;
	return first;
}

}; /* namespace LIST */

