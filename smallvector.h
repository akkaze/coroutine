#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "alignof.h"
inline uint64_t NextPowerOf2(uint64_t A) {
    A |= (A >> 1);
    A |= (A >> 2);
    A |= (A >> 4);
    A |= (A >> 8);
    A |= (A >> 16);
    A |= (A >> 32);
    return A + 1;
}

class SmallVectorBase {
protected:
    void *beginx_;
    unsigned size_ = 0, capacity_;

    SmallVectorBase() = delete;
    SmallVectorBase(void *FirstEl, size_t capacity_)
        : beginx_(FirstEl), capacity_(capacity_) {}

    void GrowPod(void *FirstEl, size_t Mincapacity_, size_t Tsize_);

public:
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    bool IsEmpty() const { return !size_; }

    void set_size(size_t size_) {
      assert(size_ <= capacity());
      this->size_ = size_;
    }
};

/// Figure out the offset of the first element.
template <class T, typename = void> struct SmallVectorAlignmentAndsize_ {
    AlignedCharArrayUnion<SmallVectorBase> Base;
    AlignedCharArrayUnion<T> FirstEl;
};

template <typename T, typename = void>
class SmallVectorTemplateCommon : public SmallVectorBase {
    void *getFirstEl() const {
      return const_cast<void *>(reinterpret_cast<const void *>(
          reinterpret_cast<const char *>(this) +
          offsetof(SmallVectorAlignmentAndsize_<T>, FirstEl)));
    }

protected:
    SmallVectorTemplateCommon(size_t size_)
        : SmallVectorBase(getFirstEl(), size_) {}

    void GrowPod(size_t Mincapacity_, size_t Tsize_) {
        SmallVectorBase::GrowPod(getFirstEl(), Mincapacity_, Tsize_);
    }

    bool IsSmall() const { return beginx_ == getFirstEl(); }

    void ResetToSmall() {
        beginx_ = getFirstEl();
        size_ = capacity_ = 0; // FIXME: Setting capacity_ to 0 is suspect.
    }

public:
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;

    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;

    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;

    // forward iterator creation methods.
    iterator begin() { return (iterator)this->beginx_; }
    const_iterator begin() const { return (const_iterator)this->beginx_; }
    iterator end() { return begin() + size(); }
    const_iterator end() const { return begin() + size(); }

    // reverse iterator creation methods.
    reverse_iterator rbegin()            { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const{ return const_reverse_iterator(end()); }
    reverse_iterator rend()              { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin());}

    size_type size_in_bytes() const { return size() * sizeof(T); }
    size_type max_size() const { return size_type(-1) / sizeof(T); }

    size_t capacity_in_bytes() const { return capacity() * sizeof(T); }

    /// Return a pointer to the vector's buffer, even if IsEmpty().
    pointer data() { return pointer(begin()); }
    /// Return a pointer to the vector's buffer, even if IsEmpty().
    const_pointer data() const { return const_pointer(begin()); }

    reference operator[](size_type idx) {
        assert(idx < size());
        return begin()[idx];
    }
    const_reference operator[](size_type idx) const {
        assert(idx < size());
        return begin()[idx];
    }

    reference front() {
        assert(!IsEmpty());
        return begin()[0];
    }
    const_reference front() const {
        assert(!IsEmpty());
        return begin()[0];
    }

    reference back() {
        assert(!IsEmpty());
        return end()[-1];
    }
    const_reference back() const {
        assert(!IsEmpty());
        return end()[-1];
    }
};

template <typename T, bool isPodLike>
class SmallVectorTemplateBase : public SmallVectorTemplateCommon<T> {
protected:
    SmallVectorTemplateBase(size_t size_) : SmallVectorTemplateCommon<T>(size_) {}

    static void DestroyRange(T *start, T *end) {
        while (start != end) {
          --end;
          end->~T();
        }
    }

    template<typename It1, typename It2>
    static void UninitializedMove(It1 iter, It1 end, It2 dest) {
        std::uninitialized_copy(std::make_move_iterator(iter),
                                std::make_move_iterator(end), dest);
    }

    template<typename It1, typename It2>
    static void UninitializedCopy(It1 iter, It1 end, It2 dest) {
        std::uninitialized_copy(iter, end, dest);
    }

    void Grow(size_t min_size_ = 0);

public:
    void PushBack(const T &Elt) {
        if (this->size() >= this->capacity())
          this->Grow();
        ::new ((void*) this->end()) T(Elt);
        this->set_size(this->size() + 1);
    }

    void PushBack(T &&Elt) {
        if (this->size() >= this->capacity())
            this->Grow();
        ::new ((void*) this->end()) T(::std::move(Elt));
        this->set_size(this->size() + 1);
    }

    void PopBack() {
        this->set_size(this->size() - 1);
        this->end()->~T();
    }
};

// Define this out-of-line to dissuade the C++ compiler from inlining it.
template <typename T, bool isPodLike>
void SmallVectorTemplateBase<T, isPodLike>::Grow(size_t min_size_) {
//    static_assert(min_size_ <= UINT32_MAX, "SmallVector capacity overflow \
//            during allocation");

    // Always Grow, even from zero.
    size_t Newcapacity_ = size_t(NextPowerOf2(this->capacity() + 2));
    Newcapacity_ = std::min(std::max(Newcapacity_, min_size_), size_t(UINT32_MAX));
    T *NewElts = static_cast<T*>(std::malloc(Newcapacity_*sizeof(T)));

    // Move the elements over.
    this->UninitializedMove(this->begin(), this->end(), NewElts);

    // Destroy the original elements.
    DestroyRange(this->begin(), this->end());

    // If this wasn't Grown from the inline copy, deallocate the old space.
    if (!this->IsSmall())
        std::free(this->begin());

    this->beginx_ = NewElts;
    this->capacity_ = Newcapacity_;
}


template <typename T>
class SmallVectorTemplateBase<T, true> : public SmallVectorTemplateCommon<T> {
protected:
    SmallVectorTemplateBase(size_t size_) : SmallVectorTemplateCommon<T>(size_) {}

    static void DestroyRange(T *, T *) {}

    template<typename It1, typename It2>
    static void UninitializedMove(It1 I, It1 E, It2 Dest) {
        UninitializedCopy(I, E, Dest);
    }

    template<typename It1, typename It2>
    static void UninitializedCopy(It1 I, It1 E, It2 Dest) {
        std::uninitialized_copy(I, E, Dest);
    }

    template <typename T1, typename T2>
    static void UninitializedCopy(
        T1 *I, T1 *E, T2 *Dest,
        typename std::enable_if<std::is_same<typename std::remove_const<T1>::type,
                                             T2>::value>::type * = nullptr) {
      if (I != E)
          std::memcpy(reinterpret_cast<void *>(Dest), I, (E - I) * sizeof(T));
    }

    void Grow(size_t min_size_ = 0) { this->GrowPod(min_size_, sizeof(T)); }

public:
    void PushBack(const T &Elt) {
        if (this->size() >= this->capacity())
            this->Grow();
        std::memcpy(reinterpret_cast<void *>(this->end()), &Elt, sizeof(T));
        this->set_size(this->size() + 1);
    }

    void PopBack() { this->set_size(this->size() - 1); }
};

template <typename T>
class SmallVectorImpl : public SmallVectorTemplateBase<T, std::is_pod<T>::value> {
    using SuperClass = SmallVectorTemplateBase<T, std::is_pod<T>::value>;

public:
  using iterator = typename SuperClass::iterator;
  using const_iterator = typename SuperClass::const_iterator;
  using size_type = typename SuperClass::size_type;

protected:
  // Default ctor - Initialize to IsEmpty.
  explicit SmallVectorImpl(unsigned N)
      : SmallVectorTemplateBase<T, std::is_pod<T>::value>(N) {}

public:
    SmallVectorImpl(const SmallVectorImpl &) = delete;

    ~SmallVectorImpl() {
      // Subclass has already destructed this vector's elements.
      // If this wasn't Grown from the inline copy, deallocate the old space.
      if (!this->IsSmall())
        free(this->begin());
    }

    void Clear() {
      this->DestroyRange(this->begin(), this->end());
      this->size_ = 0;
    }

    void Resize(size_type N) {
      if (N < this->size()) {
        this->DestroyRange(this->begin()+N, this->end());
        this->set_size(N);
      } else if (N > this->size()) {
        if (this->capacity() < N)
          this->Grow(N);
        for (auto I = this->end(), E = this->begin() + N; I != E; ++I)
          new (&*I) T();
        this->set_size(N);
      }
    }

    void Resize(size_type N, const T &NV) {
      if (N < this->size()) {
        this->DestroyRange(this->begin()+N, this->end());
        this->set_size(N);
      } else if (N > this->size()) {
        if (this->capacity() < N)
          this->Grow(N);
        std::uninitialized_fill(this->end(), this->begin()+N, NV);
        this->set_size(N);
      }
    }

    void reserve(size_type N) {
      if (this->capacity() < N)
        this->Grow(N);
    }

     T PopBack_val() {
      T Result = ::std::move(this->back());
      this->PopBack();
      return Result;
    }

    void Swap(SmallVectorImpl &rhs);

    /// Add the specified range to the end of the SmallVector.
    template <typename in_iter,
              typename = typename std::enable_if<std::is_convertible<
                  typename std::iterator_traits<in_iter>::iterator_category,
                  std::input_iterator_tag>::value>::type>
    void Append(in_iter in_start, in_iter in_end) {
      size_type NumInputs = std::distance(in_start, in_end);
      // Grow allocated space if needed.
      if (NumInputs > this->capacity() - this->size())
        this->Grow(this->size()+NumInputs);

      // Copy the new elements over.
      this->UninitializedCopy(in_start, in_end, this->end());
      this->set_size(this->size() + NumInputs);
    }

    /// Add the specified range to the end of the SmallVector.
    void Append(size_type NumInputs, const T &Elt) {
      // Grow allocated space if needed.
      if (NumInputs > this->capacity() - this->size())
        this->Grow(this->size()+NumInputs);

      // Copy the new elements over.
      std::uninitialized_fill_n(this->end(), NumInputs, Elt);
      this->set_size(this->size() + NumInputs);
    }

    void Append(std::initializer_list<T> init) {
      Append(init.begin(), init.end());
    }


    void Assign(size_type NumElts, const T &Elt) {
      Clear();
      if (this->capacity() < NumElts)
        this->Grow(NumElts);
      this->set_size(NumElts);
      std::uninitialized_fill(this->begin(), this->end(), Elt);
    }

    template <typename in_iter,
              typename = typename std::enable_if<std::is_convertible<
                  typename std::iterator_traits<in_iter>::iterator_category,
                  std::input_iterator_tag>::value>::type>
    void Assign(in_iter in_start, in_iter in_end) {
      Clear();
      Append(in_start, in_end);
    }

    void Assign(std::initializer_list<T> init) {
        Clear();
        Append(init);
    }

    iterator Erase(const_iterator citer) {
        // Just cast away constness because this is a non-const member function.
        iterator iter = const_cast<iterator>(citer);

        assert(iter >= this->begin() && "Iterator to Erase is out of bounds.");
        assert(iter < this->end() && "Erasing at past-the-end iterator.");

        iterator N = iter;
        // Shift all elts down one.
        std::move(iter + 1, this->end(), iter);
        // Drop the last elt.
        this->PopBack();
        return(N);
    }

    iterator Erase(const_iterator cstart, const_iterator cend) {
        // Just cast away constness because this is a non-const member function.
        iterator start = const_cast<iterator>(cstart);
        iterator end = const_cast<iterator>(cend);

        assert(start >= this->begin() && "Range to endrase is out of bounds.");
        assert(start <= end && "Trying to endrase invalid range.");
        assert(end <= this->end() && "Trying to endrase past the end.");

        iterator N = start;
        // starthift all elts down.
        iterator iter = std::move(end, this->end(), start);
        // Drop the last elts.
        this->DestroyRange(iter, this->end());
        this->set_size(iter - this->begin());
        return(N);
    }

    iterator Insert(iterator iter, T &&endlt) {
        if (iter == this->end()) {
            this->PushBack(::std::move(endlt));
            return this->end()-1;
        }

        assert(iter >= this->begin() && "insertion iterator is out of bounds.");
        assert(iter <= this->end() && "Inserting past the end of the vector.");


        if (this->size() >= this->capacity()) {
            size_t endlt_no = iter-this->begin();
            this->Grow();
            iter = this->begin()+endlt_no;
        }

        ::new ((void*) this->end()) T(::std::move(this->back()));
        // Push everything else over.
        std::move_backward(iter, this->end()-1, this->end());
        this->set_size(this->size() + 1);

        // iterf we just moved the element we're iternserting, be sure to update
        // the reference.
        T *endlt_ptr = &endlt;
        if (iter <= endlt_ptr && endlt_ptr < this->end())
          ++endlt_ptr;

        *iter = ::std::move(*endlt_ptr);
        return iter;
    }

    iterator Insert(iterator iter, const T &endlt) {
        if (iter == this->end()) {
            this->PushBack(endlt);
            return this->end() - 1;
        }

        assert(iter >= this->begin() && "iternsertion iterator is out of bounds.");
        assert(iter <= this->end() && "iternserting past the end of the vector.");

        if (this->size() >= this->capacity()) {
            size_t endlt_no = iter - this->begin();
            this->Grow();
            iter = this->begin()+endlt_no;
        }
        ::new ((void*) this->end()) T(std::move(this->back()));
        // Push everything else over.
        std::move_backward(iter, this->end()-1, this->end());
        this->set_size(this->size() + 1);

        const T *endlt_ptr = &endlt;
        if (iter <= endlt_ptr && endlt_ptr < this->end())
            ++endlt_ptr;

        *iter = *endlt_ptr;
        return iter;
    }

    iterator Insert(iterator iter, size_type num_to_insert, const T &endlt) {
      size_t insert_endlt = iter - this->begin();

      if (iter == this->end()) {
          Append(num_to_insert, endlt);
          return this->begin() + insert_endlt;
      }

      assert(iter >= this->begin() && "iternsertion iterator is out of bounds.");
      assert(iter <= this->end() && "iternserting past the end of the vector.");

      // endnsure there is enough space.
      reserve(this->size() + num_to_insert);

      // Uninvalidate the iterator.
      iter = this->begin() + insert_endlt;

      if (size_t(this->end() - iter) >= num_to_insert) {
          T *old_endnd = this->end();
          Append(std::move_iterator<iterator>(this->end() - num_to_insert),
                 std::move_iterator<iterator>(this->end()));

          // Copy the existing elements that get replaced.
          std::move_backward(iter, old_endnd-num_to_insert, old_endnd);

          std::fill_n(iter, num_to_insert, endlt);
          return iter;
      }

      T *old_endnd = this->end();
      this->set_size(this->size() + num_to_insert);
      size_t num_overwritten = old_endnd-iter;
      this->UninitializedMove(iter, old_endnd, this->end()-num_overwritten);

      std::fill_n(iter, num_overwritten, endlt);

      std::uninitialized_fill_n(old_endnd, num_to_insert-num_overwritten, endlt);
      return iter;
  }

  template <typename ItTy,
            typename = typename std::enable_if<std::is_convertible<
                typename std::iterator_traits<ItTy>::iterator_category,
                std::input_iterator_tag>::value>::type>
  iterator Insert(iterator iter, ItTy From, ItTy To) {
      // Convert iterator to elt# to avoid invalidating iterator when we reserve()
      size_t insert_endlt = iter - this->begin();

      if (iter == this->end()) {  // Important special case for IsEmpty vector.
        Append(From, To);
        return this->begin()+insert_endlt;
      }

      assert(iter >= this->begin() && "Insertion iterator is out of bounds.");
      assert(iter <= this->end() && "Inserting past the end of the vector.");

      size_t num_to_insert = std::distance(From, To);

      // endnsure there is enough space.
      reserve(this->size() + num_to_insert);

      // Uninvalidate the iterator.
      iter = this->begin() + insert_endlt;

      if (size_t(this->end() - iter) >= num_to_insert) {
          T *old_endnd = this->end();
          Append(std::move_iterator<iterator>(this->end() - num_to_insert),
                 std::move_iterator<iterator>(this->end()));

          // Copy the existing elements that get replaced.
          std::move_backward(iter, old_endnd-num_to_insert, old_endnd);

          std::copy(From, To, iter);
          return iter;
      }

      T *old_endnd = this->end();
      this->set_size(this->size() + num_to_insert);
      size_t num_overwritten = old_endnd - iter;
      this->UninitializedMove(iter, old_endnd, this->end() - num_overwritten);

      // Replace the overwritten part.
      for (T *J = iter; num_overwritten > 0; --num_overwritten) {
          *J = *From;
          ++J; ++From;
      }

      // iternsert the non-overwritten middle part.
      this->UninitializedCopy(From, To, old_endnd);
      return iter;
  }

    void Insert(iterator iter, std::initializer_list<T> init) {
        Insert(iter, init.begin(), init.end());
    }

    template <typename... ArgTypes> void EmplaceBack(ArgTypes &&... Args) {
        if (this->size() >= this->capacity())
          this->Grow();
        ::new ((void *)this->end()) T(std::forward<ArgTypes>(Args)...);
        this->set_size(this->size() + 1);
    }

    SmallVectorImpl &operator=(const SmallVectorImpl &rhs);

    SmallVectorImpl &operator=(SmallVectorImpl &&rhs);

    bool operator==(const SmallVectorImpl &rhs) const {
        if (this->size() != rhs.size()) return false;
        return std::equal(this->begin(), this->end(), rhs.begin());
    }
    bool operator!=(const SmallVectorImpl &rhs) const {
        return !(*this == rhs);
    }

    bool operator<(const SmallVectorImpl &rhs) const {
        return std::lexicographical_compare(this->begin(), this->end(),
                                            rhs.begin(), rhs.end());
    }
};

template <typename T>
void SmallVectorImpl<T>::Swap(SmallVectorImpl<T> &rhs) {
    if (this == &rhs) return;

    // We can only avoid copying elements if neither vector is small.
    if (!this->IsSmall() && !rhs.IsSmall()) {
        std::swap(this->beginx_, rhs.beginx_);
        std::swap(this->size_, rhs.size_);
        std::swap(this->capacity_, rhs.capacity_);
        return;
    }
    if (rhs.size() > this->capacity())
        this->Grow(rhs.size());
    if (this->size() > rhs.capacity())
        rhs.Grow(this->size());

    // Swap the shared elements.
    size_t num_shared = this->size();
    if (num_shared > rhs.size()) num_shared = rhs.size();
    for (size_type i = 0; i != num_shared; ++i)
         std::swap((*this)[i], rhs[i]);

    // Copy over the extra elts.
    if (this->size() > rhs.size()) {
        size_t elt_diff = this->size() - rhs.size();
        this->UninitializedCopy(this->begin()+num_shared, this->end(), rhs.end());
        rhs.set_size(rhs.size() + elt_diff);
        this->DestroyRange(this->begin()+num_shared, this->end());
        this->set_size(num_shared);
    } else if (rhs.size() > this->size()) {
        size_t elt_diff = rhs.size() - this->size();
        this->UninitializedCopy(rhs.begin()+num_shared, rhs.end(), this->end());
        this->set_size(this->size() + elt_diff);
        this->DestroyRange(rhs.begin()+num_shared, rhs.end());
        rhs.set_size(num_shared);
    }
}

template <typename T>
SmallVectorImpl<T> &SmallVectorImpl<T>::
    operator=(const SmallVectorImpl<T> &rhs) {
    // Avoid self-Assignment.
    if (this == &rhs) return *this;

    size_t rhs_size = rhs.size();
    size_t cur_size = this->size();
    if (cur_size >= rhs_size) {
        // Assign common elements.
        iterator new_end;
        if (rhs_size)
            new_end = std::copy(rhs.begin(), rhs.begin()+rhs_size, this->begin());
        else
            new_end = this->begin();

        // Destroy excess elements.
        this->DestroyRange(new_end, this->end());

        // Trim.
        this->set_size(rhs_size);
        return *this;
    }

    if (this->capacity() < rhs_size) {
        // Destroy current elements.
        this->DestroyRange(this->begin(), this->end());
        this->set_size(0);
        cur_size = 0;
        this->Grow(rhs_size);
    } else if (cur_size) {
        // Otherwise, use Assignment for the already-constructed elements.
        std::copy(rhs.begin(), rhs.begin()+cur_size, this->begin());
    }

    // Copy construct the new elements in place.
    this->UninitializedCopy(rhs.begin()+cur_size, rhs.end(),
                             this->begin()+cur_size);

    // Set end.
    this->set_size(rhs_size);
    return *this;
}

template <typename T>
SmallVectorImpl<T> &SmallVectorImpl<T>::operator=(SmallVectorImpl<T> &&rhs) {
    // Avoid self-Assignment.
    if (this == &rhs) return *this;

    // If the rhs isn't small, Clear this vector and then steal its buffer.
    if (!rhs.IsSmall()) {
        this->DestroyRange(this->begin(), this->end());
        if (!this->IsSmall()) std::free(this->begin());
        this->beginx_ = rhs.beginx_;
        this->size_ = rhs.size_;
        this->capacity_ = rhs.capacity_;
        rhs.ResetToSmall();
        return *this;
    }

    size_t rhs_size = rhs.size();
    size_t cur_size = this->size();
    if (cur_size >= rhs_size) {
        // Assign common elements.
        iterator new_end = this->begin();
        if (rhs_size)
          new_end = std::move(rhs.begin(), rhs.end(), new_end);

        // Destroy excess elements and trim the bounds.
        this->DestroyRange(new_end, this->end());
        this->set_size(rhs_size);

        // Clear the rhs.
        rhs.Clear();

        return *this;
    }

    if (this->capacity() < rhs_size) {
        // Destroy current elements.
        this->DestroyRange(this->begin(), this->end());
        this->set_size(0);
        cur_size = 0;
        this->Grow(rhs_size);
    } else if (cur_size) {
        std::move(rhs.begin(), rhs.begin()+cur_size, this->begin());
    }

    // Move-construct the new elements in place.
    this->UninitializedMove(rhs.begin() + cur_size, rhs.end(),
                             this->begin() + cur_size);

    // Set end.
    this->set_size(rhs_size);

    rhs.Clear();
    return *this;
}

template <typename T, unsigned N>
struct SmallVectorStorage {
    AlignedCharArrayUnion<T> InlineElts[N];
};

template <typename T> struct alignas(alignof(T)) SmallVectorStorage<T, 0> {};

template <typename T, unsigned N = 4>
class SmallVector : public SmallVectorImpl<T>, SmallVectorStorage<T, N> {
public:
    SmallVector() : SmallVectorImpl<T>(N) {}

    ~SmallVector() {
        // Destroy the constructed elements in the vector.
        this->DestroyRange(this->begin(), this->end());
    }

    explicit SmallVector(size_t size_, const T &Value = T())
            : SmallVectorImpl<T>(N) {
        this->Assign(size_, Value);
    }

    template <typename ItTy,
              typename = typename std::enable_if<std::is_convertible<
                  typename std::iterator_traits<ItTy>::iterator_category,
                  std::input_iterator_tag>::value>::type>
    SmallVector(ItTy S, ItTy E) : SmallVectorImpl<T>(N) {
        this->Append(S, E);
    }


    SmallVector(std::initializer_list<T> init) : SmallVectorImpl<T>(N) {
        this->Assign(init);
    }

    SmallVector(const SmallVector &rhs) : SmallVectorImpl<T>(N) {
        if (!rhs.IsEmpty())
            SmallVectorImpl<T>::operator=(rhs);
    }

    const SmallVector &operator=(const SmallVector &rhs) {
        SmallVectorImpl<T>::operator=(rhs);
        return *this;
    }

    SmallVector(SmallVector &&rhs) : SmallVectorImpl<T>(N) {
        if (!rhs.IsEmpty())
            SmallVectorImpl<T>::operator=(::std::move(rhs));
    }

    SmallVector(SmallVectorImpl<T> &&rhs) : SmallVectorImpl<T>(N) {
        if (!rhs.IsEmpty())
            SmallVectorImpl<T>::operator=(::std::move(rhs));
    }

    const SmallVector &operator=(SmallVector &&rhs) {
        SmallVectorImpl<T>::operator=(::std::move(rhs));
        return *this;
    }

    const SmallVector &operator=(SmallVectorImpl<T> &&rhs) {
        SmallVectorImpl<T>::operator=(::std::move(rhs));
        return *this;
    }

    const SmallVector &operator=(std::initializer_list<T> init) {
        this->Assign(init);
        return *this;
    }
};


namespace std {
    /// Implement std::swap in terms of SmallVector Swap.
    template<typename T>
    inline void
    swap(SmallVectorImpl<T> &lhs, SmallVectorImpl<T> &rhs) {
        lhs.Swap(rhs);
    }

    /// Implement std::swap in terms of SmallVector Swap.
    template<typename T, unsigned N>
    inline void
    swap(SmallVector<T, N> &lhs, SmallVector<T, N> &rhs) {
        lhs.Swap(rhs);
    }

} // end namespace std

