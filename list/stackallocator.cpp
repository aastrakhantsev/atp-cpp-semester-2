#include <iostream>


template<size_t N>
class StackStorage {
private:
  char memory_[N];
  char* ptr_ = memory_;

public:
  StackStorage() = default;

  StackStorage(const StackStorage&) = delete;

  ~StackStorage() = default;

  StackStorage& operator=(const StackStorage&) = delete;

  template<typename T>
  T* allocate(size_t capacity) {
    ptr_ += alignof(T) - (ptr_ - memory_) % alignof(T);
    T* begin = reinterpret_cast<T*>(ptr_);
    ptr_ += capacity * sizeof(T);
    return begin;
  }

  void deallocate(char*, size_t) {}
};


template<typename T, size_t N>
class StackAllocator {
private:
  StackStorage<N>* storage_;

public:
  using value_type = T;
  template<typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  StackAllocator() = delete;

  StackAllocator(StackStorage<N>& storage) noexcept : storage_(&storage) {}

  template<typename U>
  StackAllocator(const StackAllocator<U, N>& other) noexcept : storage_(other.storage_) {}

  ~StackAllocator() = default;

  template<typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& other) noexcept {
    storage_ = other.storage_;
    return *this;
  }

  T* allocate(size_t capacity) {
    T* result = storage_->template allocate<T>(capacity);
    return result;
  }
  void deallocate(T* ptr, size_t capacity) {
    storage_->deallocate(reinterpret_cast<char*>(ptr), capacity * sizeof(T));
  }

  template<typename U, size_t M>
  friend class StackAllocator;
};


template<typename T, typename Alloc = std::allocator<T>>
class List {
private:
  struct Base {
    Base* prev_ = this;
    Base* next_ = this;

    Base() : prev_(this), next_(this) {}

    ~Base() = default;
  };
  struct Node : Base {
    T value_;

    Node() = default;

    explicit Node(const T& value) : value_(value) {}

    ~Node() = default;
  };

  using node_alloc = typename Alloc::template rebind<Node>::other;
  using AllocTraits = std::allocator_traits<node_alloc>;

  Base fakeNode_;
  node_alloc alloc_;
  size_t size_ = 0;

  template<bool isConst>
  class Iterator {
  private:
    Base* ptr_;

  public:
    using value_type = T;
    using pointer = typename std::conditional<isConst, const T*, T*>::type;
    using reference = typename std::conditional<isConst, const T&, T&>::type;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = ssize_t;

    Iterator() = default;

    Iterator(Base* ptr) : ptr_(ptr) {}

    ~Iterator() = default;

    operator Iterator<true>() const {
      return Iterator<true>(ptr_);
    }

    Iterator<isConst> operator++(int) {
      Iterator<isConst> tmp(*this);
      ++(*this);
      return tmp;
    }

    Iterator<isConst>& operator++() {
      ptr_ = ptr_->next_;
      return *this;
    }

    Iterator<isConst> operator--(int) {
      Iterator<isConst> tmp(*this);
      --(*this);
      return tmp;
    }

    Iterator<isConst>& operator--() {
      ptr_ = ptr_->prev_;
      return *this;
    }

    bool operator==(const Iterator<isConst>& it) const {
      return ptr_ == it.ptr_;
    }

    bool operator!=(const Iterator<isConst>& it) const {
      return ptr_ != it.ptr_;
    }

    reference operator*() const {
      return (reinterpret_cast<Node*>(ptr_))->value_;
    }

    pointer operator->() const {
      return &(operator*());
    }

    Base* getPtr() {
      return ptr_;
    }
  };
  Iterator<false> begin_;
  Iterator<false> end_;

  void fake_push_back() {
    Base* current = reinterpret_cast<Base*>(AllocTraits::allocate(alloc_, 1));
    Base* prev = (end_.getPtr())->prev_;
    Base* next = reinterpret_cast<Base*>(end_.getPtr());
    try {
      AllocTraits::construct(alloc_, reinterpret_cast<Node*>(current));
      current->prev_ = prev;
      current->next_ = next;
      prev->next_ = current;
      next->prev_ = current;
      if (end_ == begin_) {
        begin_ = iterator(current);
      }
      ++size_;
    } catch (...) {
      prev->next_ = next;
      next->prev_ = prev;
      end_ = iterator(next);
      AllocTraits::destroy(alloc_, reinterpret_cast<Node*>(current));
      AllocTraits::deallocate(alloc_, reinterpret_cast<Node*>(current), 1);
      --size_;
      throw;
    }
  }

public:
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  List(): begin_(&fakeNode_), end_(&fakeNode_) {}

  List(size_t size): List<T, Alloc>() {
    for (size_t i = 0; i < size; i++) {
      fake_push_back();
    }
  }

  List(size_t size, const T& value) : List<T, Alloc>(size) {
    for (auto it = begin_; it != end_; it++) {
      *it = value;
    }
  }

  List(const Alloc& alloc) : alloc_(alloc), begin_(&fakeNode_), end_(&fakeNode_) {}

  List(size_t size, const Alloc& alloc) : List<T, Alloc>(alloc) {
    for (size_t i = 0; i < size; i++) {
      fake_push_back();
    }
  }

  List(size_t size, const T& value, const Alloc& alloc) : List<T, Alloc>(size, alloc) {
    for (auto it = begin_; it != end_; it++) {
      *it = value;
    }
  }

  List(const List& other): List(AllocTraits::select_on_container_copy_construction(other.alloc_)) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      push_back(*it);
    }
  }

  List& operator=(const List& other) {
    size_t currentSize = size_;
    Base currentFakeNode = fakeNode_;
    iterator currentBegin = begin_;
    iterator currentEnd = end_;
    node_alloc currentAlloc = alloc_;

    try {
      size_ = 0;
      fakeNode_ = Base();
      begin_ = &fakeNode_;
      end_ = &fakeNode_;
      if (AllocTraits::propagate_on_container_copy_assignment::value) {
        alloc_  = other.alloc_;
      }
      for (auto it = other.begin(); it != other.end(); it++) {
        push_back(*it);
      }
    } catch (...) {
      while (size_ > 0) {
        pop_back();
      }
      size_ = currentSize;
      fakeNode_ = currentFakeNode;
      begin_ = currentBegin;
      end_ = currentEnd;
      alloc_ = currentAlloc;
      throw(std::exception());
    }

    for (iterator it = currentBegin; it != currentEnd; it++) {
      AllocTraits::destroy(currentAlloc, reinterpret_cast<Node*>(it.getPtr()));
    }
    AllocTraits::deallocate(currentAlloc, reinterpret_cast<Node*>(currentBegin.getPtr()), currentSize);
    return *this;
  }

  ~List() {
    while(size_ > 0) {
      pop_back();
      // AllocTraits::deallocate(alloc_, it.getPtr(), alignof(Node));
    }
  }

  node_alloc get_allocator() const {
    return alloc_;
  }

  size_t size() const {
    return size_;
  }

  void push_back(const T& value) {
    insert(end_, value);
  }

  void pop_back() {
    iterator pos = end_;
    pos--;
    erase(pos);
  }

  void push_front(const T& value) {
    insert(begin_, value);
  }

  void pop_front() {
    erase(begin_);
  }

  iterator begin() {
    return begin_;
  }

  iterator end() {
    return end_;
  }

  const_iterator cbegin() const {
    return static_cast<const_iterator>(begin_);
  }

  const_iterator cend() const {
    return static_cast<const_iterator>(end_);
  }

  const_iterator begin() const {
    return cbegin();
  }

  const_iterator end() const {
    return cend();
  }

  reverse_iterator rbegin(){
    return reverse_iterator(end());
  }

  reverse_iterator rend() {
    return reverse_iterator(begin());
  }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(List<T, Alloc>::cend());
  }

  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  }

  void insert(const_iterator it, const T& value) {
    Base* current = reinterpret_cast<Base*>(AllocTraits::allocate(alloc_, 1));
    Base* prev = (it.getPtr())->prev_;
    Base* next = reinterpret_cast<Base*>(it.getPtr());
    try {
      AllocTraits::construct(alloc_, reinterpret_cast<Node*>(current), value);
      current->prev_ = prev;
      current->next_ = next;
      prev->next_ = current;
      next->prev_ = current;
      if (it == begin_) {
        begin_ = iterator(current);
      }
      ++size_;
    } catch (...) {
      prev->next_ = next;
      next->prev_ = prev;
      if (iterator(current) == begin_) {
        begin_ = next;
      }
      AllocTraits::destroy(alloc_, reinterpret_cast<Node*>(current));
      AllocTraits::deallocate(alloc_, reinterpret_cast<Node*>(current), 1);
      --size_;
      throw;
    }
  }

  void erase(const_iterator it) {
    Base* current = it.getPtr();
    Node* node = reinterpret_cast<Node*>(current);
    Base* prev = current->prev_;
    Base* next = current->next_;
    try {
      prev->next_ = next;
      next->prev_ = prev;
      AllocTraits::destroy(alloc_, node);
      AllocTraits::deallocate(alloc_, node, 1);
      if (it == begin_) {
        begin_ = iterator(next);
      }
      --size_;
    } catch (...) {
      Base* erased = reinterpret_cast<Base*>(AllocTraits::allocate(alloc_, 1));
      AllocTraits::construct(alloc_, reinterpret_cast<Node*>(erased), node->value_);
      erased->prev_ = prev;
      erased->next_ = next;
      prev->next_ = erased;
      next->prev_ = erased;
      if (iterator(next) == begin_) {
        begin_ = erased;
      }
      ++size_;
      throw;
    }
  }
};
