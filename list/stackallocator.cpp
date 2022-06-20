#include <iostream>
#include <memory>


template<size_t N>
class StackStorage {
private:
  char memory_[N];
  char* ptr_ = memory_;
  size_t size_ = N;

public:
  StackStorage() noexcept = default;

  StackStorage(const StackStorage&) noexcept = delete;

  ~StackStorage() noexcept = default;

  StackStorage& operator=(const StackStorage&) noexcept = delete;

  template<typename T>
  T* allocate(size_t capacity) noexcept {
    ptr_ = reinterpret_cast<char*>(std::align(alignof(T), sizeof(T), reinterpret_cast<void*&>(ptr_), size_));
    T* begin = reinterpret_cast<T*>(ptr_);
    ptr_ += capacity * sizeof(T);
    return begin;
  }

  void deallocate(char*, size_t) noexcept {}
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

  StackAllocator() noexcept = delete;

  StackAllocator(StackStorage<N>& storage) noexcept : storage_(&storage) {}

  template<typename U>
  StackAllocator(const StackAllocator<U, N>& other) noexcept : storage_(other.storage_) {}

  ~StackAllocator() noexcept = default;

  template<typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& other) noexcept {
    storage_ = other.storage_;
    return *this;
  }

  T* allocate(size_t capacity) noexcept {
    T* result = storage_->template allocate<T>(capacity);
    return result;
  }
  void deallocate(T* ptr, size_t capacity) noexcept {
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

    Base() noexcept : prev_(this), next_(this) {}

    ~Base() noexcept = default;
  };
  struct Node : Base {
    T value_;

    Node() noexcept = default;

    explicit Node(const T& value) noexcept : value_(value) {}

    ~Node() noexcept = default;
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

    Iterator() noexcept = default;

    Iterator(Base* ptr) noexcept : ptr_(ptr) {}

    ~Iterator() noexcept = default;

    operator Iterator<true>() const noexcept {
      return Iterator<true>(ptr_);
    }

    Iterator<isConst> operator++(int) noexcept {
      Iterator<isConst> tmp(*this);
      ++(*this);
      return tmp;
    }

    Iterator<isConst>& operator++() noexcept {
      ptr_ = ptr_->next_;
      return *this;
    }

    Iterator<isConst> operator--(int) noexcept {
      Iterator<isConst> tmp(*this);
      --(*this);
      return tmp;
    }

    Iterator<isConst>& operator--() noexcept {
      ptr_ = ptr_->prev_;
      return *this;
    }

    bool operator==(const Iterator<isConst>& it) const noexcept {
      return ptr_ == it.ptr_;
    }

    bool operator!=(const Iterator<isConst>& it) const noexcept {
      return ptr_ != it.ptr_;
    }

    reference operator*() const noexcept {
      return (reinterpret_cast<Node*>(ptr_))->value_;
    }

    pointer operator->() const noexcept {
      return &(operator*());
    }

    Base* getPtr() noexcept {
      return ptr_;
    }
  };

  Iterator<false> begin_;
  Iterator<false> end_;

public:
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  List() noexcept : List<T, Alloc>(0) {}

  List(size_t size) noexcept : List<T, Alloc>(size, T()) {}

  List(size_t size, const T& value) {
    for (size_t i = 0; i < size; i++) {
      try {
        push_back(value);
      } catch (...) {
        for (size_t j = 0; j < i; j++) {
          pop_back();
        }
        throw;
      }
    }
  }

  List(const Alloc& alloc) noexcept : alloc_(alloc), begin_(&fakeNode_), end_(&fakeNode_) {}

  List(size_t size, const Alloc& alloc) : List<T, Alloc>(alloc) {
    for (size_t i = 0; i < size; i++) {
      try {
        push_back(T());
      } catch (...) {
        for (size_t j = 0; j < i; j++) {
          pop_back();
        }
        throw;
      }
    }
  }

  List(size_t size, const T& value, const Alloc& alloc) {
    for (size_t i = 0; i < size; i++) {
      try {
        push_back(value);
      } catch (...) {
        for (size_t j = 0; j < i; j++) {
          pop_back();
        }
        throw;
      }
    }
    size_ = size;
    alloc_ = alloc;
    begin_ = iterator(fakeNode_.next_);
    end_ = iterator(&fakeNode_);
  }

  List(const List& other): List(AllocTraits::select_on_container_copy_construction(other.alloc_)) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      try {
        push_back(*it);
      } catch (...) {
        while (begin_ != end_) {
          pop_back();
        }
        throw;
      }
    }
  }

  List& operator=(const List& other) {
    if (AllocTraits::propagate_on_container_copy_assignment::value) {
      alloc_ = other.alloc_;
    }
    size_t added = 0;
    size_t prevSize = size_;
    try {
      for (auto it = other.begin(); it != other.end(); it++) {
        push_back(*it);
        added++;
      }
      for (size_t i = 0; i < prevSize; i++) {
        pop_front();
      }
    } catch (...) {
      for (size_t i = 0; i < added; i++) {
        pop_back();
      }
    }
    return *this;
  }

  ~List() noexcept {
    while(size_ > 0) {
      pop_back();
    }
    size_ = 0;
  }

  node_alloc get_allocator() const noexcept {
    return alloc_;
  }

  size_t size() const noexcept {
    return size_;
  }

  void push_back(const T& value) noexcept {
    insert(const_iterator(end_), value);
  }

  void pop_back() noexcept {
    iterator pos = end_;
    pos--;
    erase(pos);
  }

  void push_front(const T& value) noexcept {
    insert(begin_, value);
  }

  void pop_front() noexcept {
    erase(begin_);
  }

  iterator begin() noexcept {
    return begin_;
  }

  iterator end() noexcept {
    return end_;
  }

  const_iterator cbegin() const noexcept {
    return static_cast<const_iterator>(begin_);
  }

  const_iterator cend() const noexcept {
    return static_cast<const_iterator>(end_);
  }

  const_iterator begin() const noexcept {
    return cbegin();
  }

  const_iterator end() const noexcept {
    return cend();
  }

  reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }

  reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }

  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(List<T, Alloc>::cend());
  }

  const_reverse_iterator crend() const noexcept {
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
      if (it == const_iterator(begin_)) {
        begin_ = iterator(current);
      }
      ++size_;
    } catch (...) {
      prev->next_ = next;
      next->prev_ = prev;
      if (iterator(current) == begin_) {
        begin_ = next;
      }
      --size_;
      throw;
    }
  }

  void erase(const_iterator it) noexcept {
    Base* current = it.getPtr();
    Node* node = reinterpret_cast<Node*>(current);
    Base* prev = current->prev_;
    Base* next = current->next_;

    prev->next_ = next;
    next->prev_ = prev;
    AllocTraits::destroy(alloc_, node);
    AllocTraits::deallocate(alloc_, node, 1);
    if (it == begin_) {
      begin_ = iterator(next);
    }
    --size_;
  }
};
