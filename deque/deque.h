#include <iostream>
#include <vector>


const size_t kNodeCapacity = 128;

template<typename T = int>
class Deque {
public:
  template<bool isConst>
  class Iterator;

  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using iterator_category = std::random_access_iterator_tag;

  Deque() noexcept : Deque<T>(0) {}

  explicit Deque(size_t size) {
    size_t nodes = (size + kNodeCapacity) / kNodeCapacity;
    for (size_t i = 0; i < nodes; i++) {
      try {
        T* t = reinterpret_cast<T*>(new uint8_t[kNodeCapacity * sizeof(T)]);
        external_.push_back(t);
      } catch (...) {
        for (size_t j = 0; j < i; j++) {
          delete[] reinterpret_cast<uint8_t*>(external_[j]);
        }
        throw;
      }
    }
    size_ = size;
    begin_ = iterator(&external_, 0, 0);
    end_ = begin_ + size_;
  }

  Deque(size_t size, const T& value) : Deque<T>(size) {
    for (iterator it = begin_; it != end_; it++) {
      try {
        new(external_[it.getExternalIdx()] + it.getIdx()) T(value);
      } catch (...) {
        for (auto i = begin_; i != it; i+=kNodeCapacity) {
          delete[] reinterpret_cast<uint8_t*>(external_[i.getIdx()]);
        }
        throw;
      }
    }
  }

  Deque(const Deque<T>& other) : Deque<T>(0) {
    for (auto it = other.begin(); it != other.end(); it++) {
      try {
        push_back(*it);
      } catch (...) {
        for (auto i = begin_; i != end_; i+=kNodeCapacity) {
          delete[] reinterpret_cast<uint8_t*>(external_[i.getIdx()]);
        }
        throw;
      }
    }
  }

  ~Deque() {
    for (auto it = begin_; it != end_; ++it) {
      it->~T();
    }
    for (size_t i = 0; i < external_.size(); ++i) {
      delete[] reinterpret_cast<uint8_t*>(external_[i]);
    }
  }

  Deque<T>& operator=(const Deque<T>& other) {
    try {
      Deque<T> tmp(other);
      std::swap(external_, tmp.external_);
      std::swap(size_, tmp.size_);
      std::swap(begin_, tmp.begin_);
      begin_.setPtr(&external_);
      end_ = begin_ + size_;
      return *this;
    } catch (...) {
      throw;
    }
  }

  [[nodiscard]] size_t size() const noexcept {
    return size_;
  }

  T& operator[](int n) noexcept {
    return *(begin_ + n);
  }
  const T& operator[](int n) const noexcept {
    return *(begin_ + n);
  }
  T& at(int n) {
    if (n < 0 || (size_t)n >= size_){
      throw std::out_of_range("incorrect index :(");
    } else {
      return operator[](n);
    }
  }
  const T& at(int n) const {
    if (n < 0 || (size_t)n >= size_){
      throw std::out_of_range("incorrect index :(");
    } else {
      return operator[](n);
    }
  }

  void push_back(const T& value) {
    try {
      if ((size_t)end_.getExternalIdx() >= external_.size()) {
        reallocate(false);
      }
    } catch (...) {
      throw;
    }
    new(external_[end_.getExternalIdx()] + end_.getIdx()) T(value);
    ++size_;
    ++end_;
  }
  void pop_back() noexcept {
    --size_;
    --end_;
    end_->~T();
  }
  void push_front(const T& value) {
    try {
      if (begin_.getExternalIdx() == 0 && begin_.getIdx() == 0) {
        reallocate(true);
      }
    } catch (...) {
      throw;
    }
    iterator prev = begin_ - 1;
    new(external_[prev.getExternalIdx()] + prev.getIdx()) T(value);
    --begin_;
    ++size_;
  }
  void pop_front() noexcept {
    --size_;
    begin_->~T();
    ++begin_;
  }


  iterator begin() noexcept {
    return begin_;
  }
  iterator end() noexcept {
    return end_;
  }
  const_iterator cbegin() const noexcept {
    return const_iterator(begin_);
  }
  const_iterator cend() const noexcept {
    return const_iterator(end_);
  }
  const_iterator begin() const noexcept {
    return cbegin();
  }
  const_iterator end() const noexcept {
    return cend();
  }
  std::reverse_iterator<iterator> rbegin() noexcept {
    return std::reverse_iterator<iterator>(Deque<T>::end());
  }
  std::reverse_iterator<iterator> rend() noexcept {
    return std::reverse_iterator<iterator>(Deque<T>::begin());
  }
  std::reverse_iterator<const_iterator> crbegin() noexcept {
    return std::reverse_iterator<const_iterator>(Deque<T>::cend());
  }
  std::reverse_iterator<const_iterator> crend() noexcept {
    return std::reverse_iterator<const_iterator>(Deque<T>::cbegin());
  }

  void insert(iterator pos, const T& value) {
    try {
      if ((size_t)pos.getExternalIdx() >= external_.size()) {
        reallocate(false);
      }
    } catch (...) {
      throw;
    }
    ++end_;
    for (iterator it = end_; it != pos; it--) {
      std::swap(*it, *(it - 1));
    }
    *pos = value;
    ++size_;
  }

  void erase(iterator pos) noexcept {
    for (iterator it = pos; it != end_ - 1; it++) {
      std::swap(*it, *(it + 1));
    }
    --size_;
    --end_;
    end_->~T();
  }

private:
  std::vector<T*> external_;
  size_t size_ = 0;
  iterator begin_;
  iterator end_;

  void reallocate(bool reallocateLeft) {
    std::vector<T*> newExternal;
    if (reallocateLeft) {
      for (size_t i = 0; i < external_.size(); i++) {
        try {
          T* t = reinterpret_cast<T*>(new uint8_t[kNodeCapacity * sizeof(T)]);
          newExternal.push_back(t);
        } catch (...) {
          for (size_t j = 0; j < newExternal.size(); j++) {
            delete[] reinterpret_cast<uint8_t*>(external_[j]);
          }
          throw;
        }
      }
    }
    for (size_t i = 0; i < external_.size(); i++) {
      try {
        newExternal.push_back(external_[i]);
      } catch (...) {
        for (size_t j = 0; j < newExternal.size(); j++) {
          delete[] reinterpret_cast<uint8_t*>(external_[j]);
        }
        throw;
      }
    }
    if (!reallocateLeft) {
      for (size_t i = 0; i < external_.size(); i++) {
        try {
          T* t = reinterpret_cast<T*>(new uint8_t[kNodeCapacity * sizeof(T)]);
          newExternal.push_back(t);
        } catch (...) {
          for (size_t j = 0; j < newExternal.size(); j++) {
            delete[] reinterpret_cast<uint8_t*>(external_[j]);
          }
          throw;
        }
      }
    }
    std::swap(external_, newExternal);
    if (reallocateLeft) {
      begin_ = iterator(&external_, external_.size() / 2, 0);
      end_ = iterator(&external_, end_.getExternalIdx() + external_.size() / 2, end_.getIdx());
    } else {
      end_ = iterator(&external_, external_.size() / 2, 0);
    }
  }
};

template<typename T>
template<bool isConst>
class Deque<T>::Iterator {
public:
  using value_type = T;
  using pointer = typename std::conditional<isConst, const T*, T*>::type;
  using reference = typename std::conditional<isConst, const T&, T&>::type;
  using iterator_category = std::random_access_iterator_tag;

  Iterator() = default;

  Iterator(std::vector<T*>* ptr, size_t externalIdx, size_t idx):
          ptr_(ptr), externalIdx_(externalIdx), idx_(idx) {}

  Iterator(const Iterator& other): ptr_(other.ptr_), externalIdx_(other.externalIdx_), idx_(other.idx_) {}

  ~Iterator() = default;

  operator Iterator<true>() const {
    return Iterator<true>(ptr_, externalIdx_, idx_);
  }

  size_t getExternalIdx() {
    return externalIdx_;
  }
  size_t getIdx() {
    return idx_;
  }
  void setPtr(std::vector<T*>* ptr) {
    ptr_ = ptr;
  }

  const Iterator<isConst> operator++(int) {
    Iterator tmp(*this);
    *this += 1;
    return tmp;
  }
  Iterator<isConst>& operator++() {
    *this += 1;
    return *this;
  }
  const Iterator<isConst> operator--(int) {
    Iterator tmp(*this);
    *this -= 1;
    return tmp;
  }
  Iterator<isConst>& operator--() {
    *this -= 1;
    return *this;
  }
  Iterator<isConst>& operator+=(int n) {
    if (n < 0) return *this -= -n;
    if (idx_ + (size_t)n < kNodeCapacity) {
      idx_ += n;
    } else {
      n -= kNodeCapacity - idx_ - 1;
      externalIdx_ += (n - 1) / kNodeCapacity + 1;
      idx_ = (n - 1) % kNodeCapacity;
    }
    return *this;
  }
  Iterator<isConst>& operator-=(int n) {
    if (n < 0) return *this += -n;
    if ((int)idx_ - n >= 0) {
      idx_ -= n;
    } else {
      n -= idx_;
      externalIdx_ -= (n - 1) / kNodeCapacity + 1;
      idx_ = kNodeCapacity - ((n - 1) % kNodeCapacity) - 1;
    }
    return *this;
  }
  Iterator<isConst> operator+(int n) const {
    Iterator ret(*this);
    ret += n;
    return ret;
  }
  Iterator<isConst> operator-(int n) const {
    Iterator ret(*this);
    ret -= n;
    return ret;
  }

  bool operator<(Iterator<isConst> it) {
    return (externalIdx_ < it.getExternalIdx() || (externalIdx_ == it.getExternalIdx() && idx_ < it.getIdx()));
  }
  bool operator==(Iterator<isConst> it) {
    return !(*this < it) && !(it < *this);
  }
  bool operator>(Iterator<isConst> it) {
    return !(*this < it || *this == it);
  }
  bool operator<=(Iterator<isConst> it) {
    return !(it > *this);
  }
  bool operator>=(Iterator<isConst> it) {
    return !(*this < it);
  }
  bool operator!=(Iterator<isConst> it) {
    return (*this < it || it < *this);
  }

  size_t operator-(Iterator<isConst> it) {
    size_t result = 0;
    if (*this < it) {
      result = -(it - *this);
    } else if (externalIdx_ == it.externalIdx_) {
      result = idx_ - it.idx_;
    } else {
      result = (size_t)(abs((int)externalIdx_ - (int)it.externalIdx_) - 1) * kNodeCapacity + (kNodeCapacity - it.idx_) + idx_;
    }
    return result;
  }

  reference operator*() const {
    return (*ptr_)[externalIdx_][idx_];
  }
  pointer operator->() const {
    return &(operator*());
  }

private:
  std::vector<T*>* ptr_;
  size_t externalIdx_ = 0;
  size_t idx_ = 0;
};
