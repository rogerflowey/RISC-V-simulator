#pragma once

#include <array>
#include <optional>
#include <iterator> // For std::forward_iterator_tag

template<typename T, size_t MAX_SIZE>
class hive {
private:
    // Each element is a pair of (is_active, value)
    // Using std::optional would also work, but this is more memory-efficient
    // as it avoids std::optional's overhead if T is large.
    std::array<std::pair<bool, T>, MAX_SIZE> elements;
    size_t current_size = 0;

    // A hint to speed up finding the next free slot for insertion.
    size_t next_free_slot_hint = 0;

public:
    // --- Standard Container Typedefs ---
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = size_t;

    // --- Iterator Class ---
    template<bool IsConst>
    class base_iterator {
    private:
        using hive_ptr_type = std::conditional_t<IsConst, const hive*, hive*>;
        using reference_type = std::conditional_t<IsConst, const_reference, reference>;
        using pointer_type = std::conditional_t<IsConst, const_pointer, pointer>;

        hive_ptr_type parent_hive = nullptr;
        size_type index = 0;

        // Helper to find the next valid (active) element
        void find_next_valid() {
            while (index < MAX_SIZE && !parent_hive->elements[index].first) {
                ++index;
            }
        }

    public:
        // Iterator traits
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = pointer_type;
        using reference = reference_type;

        base_iterator() = default;
        base_iterator(hive_ptr_type hive, size_type start_index)
            : parent_hive(hive), index(start_index) {
            if (parent_hive) {
                find_next_valid();
            }
        }

        // Allow conversion from non-const to const iterator
        operator base_iterator<true>() const {
            return base_iterator<true>(parent_hive, index);
        }

        reference operator*() const {
            return parent_hive->elements[index].second;
        }

        pointer operator->() const {
            return &parent_hive->elements[index].second;
        }

        // Pre-increment
        base_iterator& operator++() {
            if (index < MAX_SIZE) {
                ++index;
                find_next_valid();
            }
            return *this;
        }

        // Post-increment
        base_iterator operator++(int) {
            base_iterator temp = *this;
            ++(*this);
            return temp;
        }

        friend bool operator==(const base_iterator& a, const base_iterator& b) {
            return a.index == b.index && a.parent_hive == b.parent_hive;
        }

        friend bool operator!=(const base_iterator& a, const base_iterator& b) {
            return !(a == b);
        }
        
        // Allow erase(iterator) to access the index
        friend class hive;
    };

    using iterator = base_iterator<false>;
    using const_iterator = base_iterator<true>;

    // --- Container Methods ---

    // Returns an iterator to the first element
    iterator begin() {
        return iterator(this, 0);
    }

    // Returns an iterator to the element following the last element
    iterator end() {
        return iterator(this, MAX_SIZE);
    }

    // Const overloads for begin/end
    const_iterator begin() const {
        return const_iterator(this, 0);
    }

    const_iterator end() const {
        return const_iterator(this, MAX_SIZE);
    }

    const_iterator cbegin() const {
        return begin();
    }

    const_iterator cend() const {
        return end();
    }

    /**
     * @brief Inserts a new element into the first available slot.
     * @tparam U The type of the value to insert (allows perfect forwarding).
     * @param value The value to insert.
     * @return An optional containing an iterator to the newly inserted element,
     *         or std::nullopt if the hive is full.
     */
    template<typename U>
    std::optional<iterator> insert(U&& value) {
        if (full()) {
            return std::nullopt;
        }

        // Search for a free slot, starting from our hint
        for (size_t i = 0; i < MAX_SIZE; ++i) {
            size_t current_index = (next_free_slot_hint + i) % MAX_SIZE;
            if (!elements[current_index].first) {
                elements[current_index].first = true;
                elements[current_index].second = std::forward<U>(value);
                current_size++;
                // Update the hint for the next insertion
                next_free_slot_hint = current_index + 1;
                return iterator(this, current_index);
            }
        }
        
        // Should be unreachable if full() check is correct, but good for safety
        return std::nullopt; 
    }

    /**
     * @brief Removes the element at the specified iterator position.
     * @param pos An iterator to the element to remove.
     * @return An iterator to the element that followed the erased element.
     */
    iterator erase(iterator pos) {
        if (pos.parent_hive != this || pos.index >= MAX_SIZE || !elements[pos.index].first) {
            // Invalid iterator, return end()
            return end();
        }

        elements[pos.index].first = false;
        current_size--;
        
        // This newly freed slot is a great candidate for the next insertion
        next_free_slot_hint = pos.index;

        // Return an iterator to the next valid element
        return ++pos;
    }

    // --- Capacity ---
    bool empty() const noexcept {
        return current_size == 0;
    }

    bool full() const noexcept {
        return current_size == MAX_SIZE;
    }

    size_type size() const noexcept {
        return current_size;
    }

    constexpr size_type max_size() const noexcept {
        return MAX_SIZE;
    }

    void clear() noexcept {
        for(size_t i = 0; i < MAX_SIZE; ++i) {
            elements[i].first = false;
        }
        current_size = 0;
        next_free_slot_hint = 0;
    }
};