/*
    Copyright (C) 2003 by Mat Sutcliffe <oktal@gmx.co.uk>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __CS_UTIL_HASH_H__
#define __CS_UTIL_HASH_H__

#include <csutil/array.h>

/**
 * Compute a hash key for a null-terminated string.
 *
 * Note that these keys are non-unique; some dissimilar strings may generate
 * the same key. For unique keys, see csStringSet.
 */
uint32 csHashCompute (char const*);

/**
 * Compute a hash key for a string of a given length.
 *
 * Note that these keys are non-unique; some dissimilar strings may generate
 * the same key. For unique keys, see csStringSet.
 */
uint32 csHashCompute (char const*, int length);

template <class T>
class csIntegralHashKeyHandler
{
public:
  static uint32 ComputeHash (const T& key)
  {
    return (uint32)key;
  }

  static bool CompareKeys (const T& key1, const T& key2)
  {
    return (key1 == key2);
  }
};

/**
 * A generic hash table class,
 * which grows dynamically and whose buckets are unsorted arrays.
 */
template <class T, class K = uint32, 
  class KeyHandler = csIntegralHashKeyHandler<K> > 
class csHash
{
protected:
  struct Element
  {
    K key;
    T value;

    Element (K key0, const T &value0) : key (key0), value (value0) {}
    Element (const Element &other) : key (other.key), value (other.value) {}
  };
  csArray< csArray<Element> > Elements;

  int Modulo;

private:
  int InitModulo;
  int GrowRate;
  int MaxSize;
  int Size;

  void Grow ()
  {
    static const int Primes[] =
    {
      53,         97,         193,       389,       769, 
      1543,       3079,       6151,      12289,     24593,
      49157,      98317,      196613,    393241,    786433,
      1572869,    3145739,    6291469,   12582917,  25165843,
      50331653,   100663319,  201326611, 402653189, 805306457,
      1610612741, 0
    };

    const int *p;
    for (p = Primes; *p && *p <= Elements.Length (); p++) ;
    Modulo = *p;
    CS_ASSERT (Modulo);

    int elen = Elements.Length ();
    Elements.SetLength (Modulo, csArray<Element> (1, 7));

    for (int i = 0; i < elen; i++)
    {
      csArray<Element>& src = Elements[i];
      int slen = src.Length ();
      for (int j = slen - 1; j >= 0; j--)
      {
        const Element& srcElem = src[j];
        csArray<Element>& dst = 
	  Elements.Get (KeyHandler::ComputeHash (srcElem.key) % Modulo);
        if (&src != &dst)
        {
          dst.Push (srcElem);
          src.DeleteIndex (j);
        }
      }
    }
  }

public:
  /**
   * Construct a hash table with an array of the given size,
   * which for optimisation reasons should be a prime number.
   * 
   * Grow_rate is the rate at which the hash table grows:
   * Size doubles once there are size/grow_rate collisions.
   * It will not grow after it reaches max_size.
   *
   * Here are a few primes: 7, 11, 19, 29, 59, 79, 101, 127, 151, 199, 251,
   * 307, 401, 503, 809, 1009, 1499, 2003, 3001, 5003, 12263, 25247, 36923,
   * 50119, 70951, 90313, 104707.
   * For a bigger list go to http://www.utm.edu/research/primes/
   */
  csHash (int size = 257, int grow_rate = 64, int max_size = 20000)
    : Elements (size), Modulo (size), InitModulo (size),
      GrowRate (grow_rate), MaxSize (max_size), Size (0)
  {
    Elements.SetLength (size, csArray<Element> (1, 7));
  }

  /// Copy constructor.
  csHash (const csHash<T> &o) : Elements (o.Elements),
    Modulo (o.Modulo), InitModulo (o.InitModulo),
    GrowRate (o.GrowRate), MaxSize (o.MaxSize), Size (o.Size) {}

  /// Add an element to the hash table.
  void Put (K key, const T &value)
  {
    csArray<Element> &values = 
      Elements[KeyHandler::ComputeHash (key) % Modulo];
    values.Push (Element (key, value));
    Size++;
    if (values.Length () > Elements.Length () / GrowRate
     && Elements.Length () < MaxSize) Grow ();
  }

  /// Get all the elements with the given key, or empty if there are none.
  csArray<T> GetAll (uint32 key) const
  {
    const csArray<Element> &values = 
      Elements[KeyHandler::ComputeHash (key) % Modulo];
    csArray<T> ret (values.Length () / 2);
    for (int i = values.Length () - 1; i >= 0; i--)
      if (KeyHandler::CompareKeys (values[i].key, key)) 
	ret.Push (values[i].value);
    return ret;
  }

  /// Add an element to the hash table, overwriting if the key already exists.
  void PutFirst (K key, const T &value)
  {
    csArray<Element> &values = 
      Elements[KeyHandler::ComputeHash (key) % Modulo];
    for (int i = values.Length () - 1; i >= 0; i--)
      if (KeyHandler::CompareKeys (values[i].key, key))
      {
        values[i].value = value;
        return;
      }

    values.Push (Element (key, value));
    Size++;
    if (values.Length () > Elements.Length () / GrowRate
     && Elements.Length () < MaxSize) Grow ();
  }

  /// Get the first element matching the given key, or 0 if there is none.
  const T& Get (K key) const
  {
    const csArray<Element> &values = 
      Elements[KeyHandler::ComputeHash (key) % Modulo];
    for (int i = values.Length () - 1; i >= 0; i--)
      if (KeyHandler::CompareKeys (values[i].key, key)) 
	return values[i].value;

    static const T zero (0);
    return zero;
  }

  /// Delete all the elements, returning false if there are none.
  bool DeleteAll ()
  {
    bool ret = false;
    int elen = Elements.Length ();
    Elements.SetLength (Modulo = InitModulo);
    for (int i = 0; i < elen; i++)
    {
      if (Elements[i].Length ()) ret = true;
      Elements[i].Empty ();
    }
    Size = 0;
    return ret;
  }

  /// Delete all the elements matching the given key.
  bool DeleteAll (K key)
  {
    bool ret = false;
    csArray<Element> &values = 
      Elements[KeyHandler::ComputeHash (key) % Modulo];
    for (int i = values.Length () - 1; i >= 0; i--)
      if (KeyHandler::CompareKeys (values[i].key, key))
      {
        values.DeleteIndex (i);
        ret = true;
        Size--;
      }
    return ret;
  }
  
  /// Delete all the elements matching the given key and value.
  bool Delete (K key, const T &value)
  {
    bool ret = false;
    csArray<Element> &values = 
      Elements[KeyHandler::ComputeHash (key) % Modulo];
    for (int i = values.Length () - 1; i >= 0; i--)
      if (KeyHandler::CompareKeys (values[i].key, key) && 
	(values[i].value == value))
      {
        values.DeleteIndex (i);
        ret = true;
        Size--;
      }
    return ret;
  }

  /// Get the number of elements in the hash.
  int GetSize () const
  {
    return Size;
  }

  /// An iterator class for the hash.
  class Iterator
  {
  private:
    const csHash<T, K, KeyHandler>* hash;
    K key;
    int bucket, size, element;

    void Seek ()
    {
      while ((element < size) && 
        ! KeyHandler::CompareKeys (hash->Elements[bucket][element].key, key))
          element++;
    }

  protected:
    Iterator (const csHash<T, K, KeyHandler>* hash0, K key0)
    : hash (hash0), key (key0), 
      bucket (KeyHandler::ComputeHash (key) % hash->Modulo),
      size (hash->Elements[bucket].Length ()) { Return (); }

    friend class csHash<T, K, KeyHandler>;
  public:
    Iterator (const Iterator &o)
    : hash (o.hash), bucket (o.bucket), size (o.size), element (o.element) {}

    /// Returns a boolean indicating whether or not the hash has more elements.
    bool HasNext () const
    {
      return element < size;
    }

    /// Get the next element's value.
    const T& Next ()
    {
      const T &ret = hash->Elements[bucket][element].value;
      element++;
      Seek ();
      return ret;
    }

    /// Delete the next element.
    void DeleteNext ()
    {
      hash->Elements[bucket].DeleteIndex (element);
    }

    /// Move the iterator back to the first element.
    void Return () { element = 0; Seek (); }
  };
  friend class csHash<T, K, KeyHandler>::Iterator;

  /// An iterator class for the hash.
  class GlobalIterator
  {
  private:
    const csHash<T, K, KeyHandler> *hash;
    int bucket, size, element;

    void Zero () { bucket = element = 0; }
    void Init () { size = hash->Elements[bucket].Length (); }

    void FindItem ()
    {
      if (element >= size)
      {
	while (++bucket < hash->Elements.Length ())
	{
          Init ();
	  if (size != 0)
	  {
	    element = 0;
	    break;
	  }
	}
      }
    }

  protected:
    GlobalIterator (const csHash<T, K, KeyHandler> *hash0) : hash (hash0) 
    { 
      Zero (); 
      Init (); 
      FindItem ();
    }

    friend class csHash<T, K, KeyHandler>;
  public:
    GlobalIterator (const Iterator &o) : hash (o.hash), bucket (o.bucket),
      size (o.size), element (o.element) {}

    /// Returns a boolean indicating whether or not the hash has more elements.
    bool HasNext () const
    {
      if (hash->Elements.Length () == 0) return false;
      return element < size || bucket < hash->Elements.Length ();
    }

    /// Get the next element's value.
    const T& Next ()
    {
      const T &ret = hash->Elements[bucket][element].value;
      element++;
      FindItem ();
      return ret;
    }

    /// Get the next element's value and key.
    const T& Next (K &key)
    {
      key = hash->Elements[bucket][element].key;
      return Next ();
    }

    /// Delete the next element.
    void DeleteNext ()
    {
      hash->Elements[bucket].DeleteIndex (element);
    }

    /// Move the iterator back to the first element.
    void Return () { Zero (); Init (); }
  };
  friend class csHash<T, K, KeyHandler>::GlobalIterator;

  /**
   * Return an iterator for the hash, to iterate only over the elements
   * with the given key.
   * Modifying the hash (except with DeleteNext) while you have open iterators
   * will cause undefined behaviour.
   */
  Iterator GetIterator (K key) const
  {
    return Iterator (this, key);
  }

  /**
   * Return an iterator for the hash, to iterate over all elements.
   * Modifying the hash (except with DeleteNext) while you have open iterators
   * will cause undefined behaviour.
   */
  GlobalIterator GetIterator () const
  {
    return GlobalIterator (this);
  }
};

#endif
