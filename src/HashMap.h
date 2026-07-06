#ifndef PACKAGES_LIGHTGRAPH_SRC_HASHMAP_H_
#define PACKAGES_LIGHTGRAPH_SRC_HASHMAP_H_

#pragma once

//for convenience
#define CreateHashMap(hashM, ktype, vtype) HashMap<ktype,vtype> hashM
#define CreateComplexHashMap(hashM, ktype, vtype, comparator) HashMap<ktype,vtype> hashM(comparator)

template<typename K, typename V>
class HashMap
{
  public:
    typedef bool (*comparator)(K, K);

    HashMap(unsigned int capacity, comparator compare = 0) : capacity(capacity) {
      keys = new K[capacity]();
      values = new V[capacity]();
      cb_comparator = compare;
      currentIndex = 0;
    }
    
    ~HashMap() {
        delete[] keys;
        delete[] values;
    }

    unsigned int size() const {
      return currentIndex;
    }

    K keyAt(unsigned int idx) {
      return keys[idx];
    }

    V valueAt(unsigned int idx) {
      return values[idx];
    }

    bool willOverflow() const {
        return (currentIndex + 1 > capacity);
    }

    const V& operator[](const K key) const {
        int index = indexOf(key);
        if (index > -1) {
            return values[index];
        }
        return nilValue;
    }

    V& operator[](const K key) {
        if (contains(key))
        {
            return values[indexOf(key)];
        }
        else if (currentIndex < capacity)
        {
            keys[currentIndex] = key;
            values[currentIndex] = nilValue;
            currentIndex++;
            return values[currentIndex - 1];
        }
        return nilValue;
    }
    
    V& get(const K key) {
        return operator[](key);
    }
    
    void set(const K key, V value) {
        operator[](key) = value;
    }

    int indexOf(K key) const {
      for (unsigned int i = 0; i < currentIndex; i++)
      {
        if (cb_comparator)
        {
          if (cb_comparator(key, keys[i]))
          {
            return i;
          }
        }
        else
        {
          if (key == keys[i])
          {
            return i;
          }
        }
      }
      return -1;
    }

    bool contains(K key) const {
      for (unsigned int i = 0; i < currentIndex; i++)
      {
        if (cb_comparator)
        {
          if (cb_comparator(key, keys[i]))
          {
            return true;
          }
        }
        else
        {
          if (key == keys[i])
          {
            return true;
          }
        }
      }
      return false;
    }

    void remove(K key)
    {
      int index = indexOf(key);
      if (contains(key))
      {
        for (unsigned int i = index; i < capacity - 1; i++)
        {
          keys[i] = keys[i + 1];
          values[i] = values[i + 1];
        }
        currentIndex--;
      }
    }

    void setNullValue(V nullv)
    {
      nilValue = nullv;
    }

  protected:
    K *keys;
    V *values;
    V nilValue;
    unsigned int capacity;
    unsigned int currentIndex;
    comparator cb_comparator;
};

#endif  // PACKAGES_LIGHTGRAPH_SRC_HASHMAP_H_
