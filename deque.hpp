#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"
#include <cstddef>

namespace sjtu {

template <class T> class deque {
private:
    static const int BLOCK_CAPACITY = 500;
    static const int MIN_CAPACITY = 240;

    struct Node {
        deque* dq;
        Node* prev;
        Node* next;
        int head;
        int tail;
        T* data;

        Node(deque* dq) : dq(dq), prev(nullptr), next(nullptr), head(BLOCK_CAPACITY / 2), tail(BLOCK_CAPACITY / 2) {
            data = static_cast<T*>(operator new(sizeof(T) * BLOCK_CAPACITY));
        }
        ~Node() {
            for (int i = head; i < tail; ++i) {
                data[i].~T();
            }
            operator delete(data);
        }
        int size() const { return tail - head; }

        void recenter_to(int new_head) {
            if (new_head == head) return;
            int sz = size();
            if (new_head > head) {
                for (int i = sz - 1; i >= 0; --i) {
                    int src = head + i;
                    int dst = new_head + i;
                    if (dst >= tail) {
                        new (data + dst) T(data[src]);
                    } else {
                        data[dst] = data[src];
                    }
                }
                int end_destroy = (new_head < tail) ? new_head : tail;
                for (int i = head; i < end_destroy; ++i) {
                    data[i].~T();
                }
            } else {
                for (int i = 0; i < sz; ++i) {
                    int src = head + i;
                    int dst = new_head + i;
                    if (dst < head) {
                        new (data + dst) T(data[src]);
                    } else {
                        data[dst] = data[src];
                    }
                }
                int start_destroy = (new_head + sz > head) ? new_head + sz : head;
                for (int i = start_destroy; i < tail; ++i) {
                    data[i].~T();
                }
            }
            head = new_head;
            tail = new_head + sz;
        }

        void recenter() {
            recenter_to((BLOCK_CAPACITY - size()) / 2);
        }

        Node* split() {
            int sz = size();
            int half = sz / 2;
            int remain = sz - half;

            Node* new_node = new Node(dq);
            new_node->head = (BLOCK_CAPACITY - half) / 2;
            new_node->tail = new_node->head + half;

            for (int i = 0; i < half; ++i) {
                new (new_node->data + new_node->head + i) T(data[head + remain + i]);
            }

            for (int i = head + remain; i < tail; ++i) {
                data[i].~T();
            }
            tail = head + remain;

            new_node->next = next;
            new_node->prev = this;
            if (next) next->prev = new_node;
            else dq->last_node = new_node;
            next = new_node;

            recenter();
            return new_node;
        }

        void merge_with_next() {
            Node* next_node = next;
            int sz1 = size();
            int sz2 = next_node->size();

            if (tail + sz2 > BLOCK_CAPACITY) {
                int new_head = (BLOCK_CAPACITY - (sz1 + sz2)) / 2;
                recenter_to(new_head);
            }

            for (int i = 0; i < sz2; ++i) {
                new (data + tail + i) T(next_node->data[next_node->head + i]);
            }
            tail += sz2;

            next = next_node->next;
            if (next) next->prev = this;
            else dq->last_node = this;
            delete next_node;
        }

        void borrow_from_next() {
            Node* next_node = next;
            if (tail == BLOCK_CAPACITY) {
                recenter();
            }
            new (data + tail) T(next_node->data[next_node->head]);
            tail++;

            next_node->data[next_node->head].~T();
            next_node->head++;
        }

        void borrow_from_prev() {
            Node* prev_node = prev;
            if (head == 0) {
                recenter();
            }
            head--;
            new (data + head) T(prev_node->data[prev_node->tail - 1]);

            prev_node->data[prev_node->tail - 1].~T();
            prev_node->tail--;
        }

        void maintain() {
            if (size() >= MIN_CAPACITY) return;

            if (prev && prev->size() > MIN_CAPACITY) {
                borrow_from_prev();
            } else if (next && next->size() > MIN_CAPACITY) {
                borrow_from_next();
            } else if (prev) {
                prev->merge_with_next();
            } else if (next) {
                merge_with_next();
            }
        }
    };

    Node* first_node;
    Node* last_node;
    size_t total_size;

    int get_global_index(Node* node, int idx) const {
        int g_idx = 0;
        Node* curr = first_node;
        while (curr && curr != node) {
            g_idx += curr->size();
            curr = curr->next;
        }
        if (!curr) throw invalid_iterator();
        return g_idx + idx;
    }

public:
    class const_iterator;
    class iterator {
        friend class deque;
        friend class const_iterator;
    private:
        deque* dq;
        Node* node;
        int idx;
    public:
        iterator(deque* dq = nullptr, Node* node = nullptr, int idx = 0) : dq(dq), node(node), idx(idx) {}

        iterator operator+(const int &n) const {
            if (n < 0) return operator-(-n);
            iterator res = *this;
            res += n;
            return res;
        }
        iterator operator-(const int &n) const {
            if (n < 0) return operator+(-n);
            iterator res = *this;
            res -= n;
            return res;
        }

        int operator-(const iterator &rhs) const {
            if (dq != rhs.dq) throw invalid_iterator();
            return dq->get_global_index(node, idx) - dq->get_global_index(rhs.node, rhs.idx);
        }
        iterator &operator+=(const int &n) {
            if (n < 0) return operator-=(-n);
            int rem = n;
            while (rem > 0) {
                if (!node) throw invalid_iterator();
                int remain = node->size() - idx;
                if (rem < remain) {
                    idx += rem;
                    rem = 0;
                } else {
                    rem -= remain;
                    if (node->next) {
                        node = node->next;
                        idx = 0;
                    } else {
                        if (rem == 0) {
                            idx = node->size();
                        } else {
                            throw invalid_iterator();
                        }
                    }
                }
            }
            if (node && idx == node->size() && node->next) {
                node = node->next;
                idx = 0;
            }
            return *this;
        }
        iterator &operator-=(const int &n) {
            if (n < 0) return operator+=(-n);
            int rem = n;
            while (rem > 0) {
                if (!node) throw invalid_iterator();
                if (rem <= idx) {
                    idx -= rem;
                    rem = 0;
                } else {
                    rem -= (idx + 1);
                    if (node->prev) {
                        node = node->prev;
                        idx = node->size() - 1;
                    } else {
                        throw invalid_iterator();
                    }
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        iterator &operator++() {
            if (!node || (idx == node->size() && !node->next)) throw invalid_iterator();
            idx++;
            if (idx == node->size() && node->next) {
                node = node->next;
                idx = 0;
            }
            return *this;
        }
        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }
        iterator &operator--() {
            if (!node) throw invalid_iterator();
            if (idx == 0) {
                if (node->prev) {
                    node = node->prev;
                    idx = node->size() - 1;
                } else {
                    throw invalid_iterator();
                }
            } else {
                idx--;
            }
            return *this;
        }

        T &operator*() const {
            if (!node || idx < 0 || idx >= node->size()) throw invalid_iterator();
            return node->data[node->head + idx];
        }
        T *operator->() const noexcept {
            return &(node->data[node->head + idx]);
        }

        bool operator==(const iterator &rhs) const {
            return dq == rhs.dq && node == rhs.node && idx == rhs.idx;
        }
        bool operator==(const const_iterator &rhs) const {
            return dq == rhs.dq && node == rhs.node && idx == rhs.idx;
        }
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }
        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }
    };

    class const_iterator {
        friend class deque;
        friend class iterator;
    private:
        const deque* dq;
        Node* node;
        int idx;
    public:
        const_iterator(const deque* dq = nullptr, Node* node = nullptr, int idx = 0) : dq(dq), node(node), idx(idx) {}
        const_iterator(const iterator& other) : dq(other.dq), node(other.node), idx(other.idx) {}

        const_iterator operator+(const int &n) const {
            if (n < 0) return operator-(-n);
            const_iterator res = *this;
            res += n;
            return res;
        }
        const_iterator operator-(const int &n) const {
            if (n < 0) return operator+(-n);
            const_iterator res = *this;
            res -= n;
            return res;
        }

        int operator-(const const_iterator &rhs) const {
            if (dq != rhs.dq) throw invalid_iterator();
            return dq->get_global_index(node, idx) - dq->get_global_index(rhs.node, rhs.idx);
        }
        const_iterator &operator+=(const int &n) {
            if (n < 0) return operator-=(-n);
            int rem = n;
            while (rem > 0) {
                if (!node) throw invalid_iterator();
                int remain = node->size() - idx;
                if (rem < remain) {
                    idx += rem;
                    rem = 0;
                } else {
                    rem -= remain;
                    if (node->next) {
                        node = node->next;
                        idx = 0;
                    } else {
                        if (rem == 0) {
                            idx = node->size();
                        } else {
                            throw invalid_iterator();
                        }
                    }
                }
            }
            if (node && idx == node->size() && node->next) {
                node = node->next;
                idx = 0;
            }
            return *this;
        }
        const_iterator &operator-=(const int &n) {
            if (n < 0) return operator+=(-n);
            int rem = n;
            while (rem > 0) {
                if (!node) throw invalid_iterator();
                if (rem <= idx) {
                    idx -= rem;
                    rem = 0;
                } else {
                    rem -= (idx + 1);
                    if (node->prev) {
                        node = node->prev;
                        idx = node->size() - 1;
                    } else {
                        throw invalid_iterator();
                    }
                }
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        const_iterator &operator++() {
            if (!node || (idx == node->size() && !node->next)) throw invalid_iterator();
            idx++;
            if (idx == node->size() && node->next) {
                node = node->next;
                idx = 0;
            }
            return *this;
        }
        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }
        const_iterator &operator--() {
            if (!node) throw invalid_iterator();
            if (idx == 0) {
                if (node->prev) {
                    node = node->prev;
                    idx = node->size() - 1;
                } else {
                    throw invalid_iterator();
                }
            } else {
                idx--;
            }
            return *this;
        }

        const T &operator*() const {
            if (!node || idx < 0 || idx >= node->size()) throw invalid_iterator();
            return node->data[node->head + idx];
        }
        const T *operator->() const noexcept {
            return &(node->data[node->head + idx]);
        }

        bool operator==(const iterator &rhs) const {
            return dq == rhs.dq && node == rhs.node && idx == rhs.idx;
        }
        bool operator==(const const_iterator &rhs) const {
            return dq == rhs.dq && node == rhs.node && idx == rhs.idx;
        }
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }
        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }
    };

    deque() {
        first_node = last_node = new Node(this);
        total_size = 0;
    }
    deque(const deque &other) {
        first_node = last_node = new Node(this);
        total_size = 0;
        for (const_iterator it = other.cbegin(); it != other.cend(); ++it) {
            push_back(*it);
        }
    }
    ~deque() {
        clear();
        delete first_node;
    }
    deque &operator=(const deque &other) {
        if (this == &other) return *this;
        clear();
        for (const_iterator it = other.cbegin(); it != other.cend(); ++it) {
            push_back(*it);
        }
        return *this;
    }

    T &at(const size_t &pos) {
        if (pos < 0 || pos >= total_size) throw index_out_of_bound();
        Node* curr = first_node;
        size_t p = pos;
        while (curr) {
            if (p < curr->size()) {
                return curr->data[curr->head + p];
            }
            p -= curr->size();
            curr = curr->next;
        }
        throw index_out_of_bound();
    }
    const T &at(const size_t &pos) const {
        if (pos < 0 || pos >= total_size) throw index_out_of_bound();
        Node* curr = first_node;
        size_t p = pos;
        while (curr) {
            if (p < curr->size()) {
                return curr->data[curr->head + p];
            }
            p -= curr->size();
            curr = curr->next;
        }
        throw index_out_of_bound();
    }
    T &operator[](const size_t &pos) { return at(pos); }
    const T &operator[](const size_t &pos) const { return at(pos); }

    const T &front() const {
        if (empty()) throw container_is_empty();
        return first_node->data[first_node->head];
    }
    const T &back() const {
        if (empty()) throw container_is_empty();
        return last_node->data[last_node->tail - 1];
    }

    iterator begin() { return iterator(this, first_node, 0); }
    const_iterator cbegin() const { return const_iterator(this, first_node, 0); }
    iterator end() { return iterator(this, last_node, last_node->size()); }
    const_iterator cend() const { return const_iterator(this, last_node, last_node->size()); }

    bool empty() const { return total_size == 0; }
    size_t size() const { return total_size; }

    void clear() {
        Node* curr = first_node;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
        first_node = last_node = new Node(this);
        total_size = 0;
    }

    iterator insert(iterator pos, const T &value) {
        if (pos.dq != this) throw invalid_iterator();
        Node* node = pos.node;
        int idx = pos.idx;
        if (!node) throw invalid_iterator();

        if (idx == node->size() && !node->next) {
            push_back(value);
            return iterator(this, last_node, last_node->size() - 1);
        }

        if (node->size() == BLOCK_CAPACITY) {
            Node* new_node = node->split();
            if (idx > node->size()) {
                idx -= node->size();
                node = new_node;
            }
        }

        if (node->head > 0 && (node->tail == BLOCK_CAPACITY || idx < node->size() / 2)) {
            node->head--;
            if (idx > 0) {
                new (node->data + node->head) T(node->data[node->head + 1]);
                for (int i = 1; i < idx; ++i) {
                    node->data[node->head + i] = node->data[node->head + i + 1];
                }
                node->data[node->head + idx] = value;
            } else {
                new (node->data + node->head) T(value);
            }
        } else {
            if (idx < node->size()) {
                new (node->data + node->tail) T(node->data[node->tail - 1]);
                for (int i = node->size() - 1; i > idx; --i) {
                    node->data[node->head + i] = node->data[node->head + i - 1];
                }
                node->data[node->head + idx] = value;
            } else {
                new (node->data + node->tail) T(value);
            }
            node->tail++;
        }
        total_size++;
        return iterator(this, node, idx);
    }

    iterator erase(iterator pos) {
        if (pos.dq != this || !pos.node || pos.idx < 0 || pos.idx >= pos.node->size()) {
            throw invalid_iterator();
        }
        Node* node = pos.node;
        int idx = pos.idx;

        if (idx < node->size() / 2) {
            for (int i = idx; i > 0; --i) {
                node->data[node->head + i] = node->data[node->head + i - 1];
            }
            node->data[node->head].~T();
            node->head++;
        } else {
            for (int i = idx; i < node->size() - 1; ++i) {
                node->data[node->head + i] = node->data[node->head + i + 1];
            }
            node->data[node->tail - 1].~T();
            node->tail--;
        }
        total_size--;

        int g_idx = get_global_index(node, idx < node->size() / 2 ? idx : idx);

        if (node->size() < MIN_CAPACITY && (node->prev || node->next)) {
            node->maintain();
        }

        if (g_idx == total_size) return end();
        
        Node* curr = first_node;
        int p = g_idx;
        while (curr) {
            if (p < curr->size()) {
                return iterator(this, curr, p);
            }
            p -= curr->size();
            curr = curr->next;
        }
        throw invalid_iterator();
    }

    void push_back(const T &value) {
        if (last_node->tail == BLOCK_CAPACITY) {
            if (last_node->size() < BLOCK_CAPACITY) {
                last_node->recenter();
            }
            if (last_node->tail == BLOCK_CAPACITY) {
                Node* new_node = new Node(this);
                new_node->prev = last_node;
                last_node->next = new_node;
                last_node = new_node;
                new_node->head = new_node->tail = 0;
            }
        }
        new (last_node->data + last_node->tail) T(value);
        last_node->tail++;
        total_size++;
    }

    void pop_back() {
        if (empty()) throw container_is_empty();
        last_node->tail--;
        last_node->data[last_node->tail].~T();
        total_size--;

        if (last_node->size() == 0 && last_node->prev) {
            Node* temp = last_node;
            last_node = last_node->prev;
            last_node->next = nullptr;
            delete temp;
        } else if (last_node->size() < MIN_CAPACITY && last_node->prev) {
            last_node->maintain();
        }
    }

    void push_front(const T &value) {
        if (first_node->head == 0) {
            if (first_node->size() < BLOCK_CAPACITY) {
                first_node->recenter();
            }
            if (first_node->head == 0) {
                Node* new_node = new Node(this);
                new_node->next = first_node;
                first_node->prev = new_node;
                first_node = new_node;
                new_node->head = new_node->tail = BLOCK_CAPACITY;
            }
        }
        first_node->head--;
        new (first_node->data + first_node->head) T(value);
        total_size++;
    }

    void pop_front() {
        if (empty()) throw container_is_empty();
        first_node->data[first_node->head].~T();
        first_node->head++;
        total_size--;

        if (first_node->size() == 0 && first_node->next) {
            Node* temp = first_node;
            first_node = first_node->next;
            first_node->prev = nullptr;
            delete temp;
        } else if (first_node->size() < MIN_CAPACITY && first_node->next) {
            first_node->maintain();
        }
    }
};

} // namespace sjtu

#endif