#ifndef CONTAINERS_H_INCLUDED
#define CONTAINERS_H_INCLUDED

#include "objects/object_pool.h"
#include "objects/dyn_object.h"
#include "utils/shared_id_set_pool.h"
#include "utils/hybrid_map.h"
#include "utils/hybrid_pool.h"
#include "fixes/linux.h"

#include "sdk/amx/amx.h"

#include <vector>
#include <unordered_map>
#include <list>
#include <memory>
#include <exception>
#include <typeinfo>
#include <functional>
#include <iterator>

template <class Type>
class collection_base
{
protected:
	Type data;
	int revision = 0;
public:
	typedef typename Type::iterator iterator;
	typedef typename Type::const_iterator const_iterator;
	typedef typename Type::reference reference;
	typedef typename Type::const_reference const_reference;
	typedef typename Type::value_type value_type;

	iterator begin()
	{
		return data.begin();
	}

	iterator end()
	{
		return data.end();
	}

	const_iterator cbegin() const
	{
		return data.cbegin();
	}

	const_iterator cend() const
	{
		return data.cend();
	}

	size_t size() const
	{
		return data.size();
	}

	void clear()
	{
		if(data.size() > 0)
		{
			data.clear();
			++revision;
		}
	}

	iterator erase(iterator position)
	{
		auto it = data.erase(position);
		++revision;
		return it;
	}

	iterator erase(iterator first, iterator last)
	{
		auto it = data.erase(first, last);
		++revision;
		return it;
	}

	template <class InputIterator>
	void insert(InputIterator first, InputIterator last)
	{
		data.insert(first, last);
		++revision;
	}

	int get_revision() const
	{
		return revision;
	}

	void swap(collection_base<Type> &other)
	{
		std::swap(data, other.data);
		std::swap(revision, other.revision);
	}
};

class list_t : public collection_base<std::vector<dyn_object>>
{
public:
	typedef typename std::vector<dyn_object>::reverse_iterator reverse_iterator;
	reverse_iterator rbegin()
	{
		return data.rbegin();
	}
	reverse_iterator rend()
	{
		return data.rend();
	}
	dyn_object &operator[](size_t index)
	{
		return data[index];
	}
	const dyn_object &operator[](size_t index) const
	{
		return data[index];
	}
	void push_back(dyn_object &&value);
	void push_back(const dyn_object &value);
	iterator insert(iterator position, dyn_object &&value);
	iterator insert(iterator position, const dyn_object &value);
	bool insert_dyn(iterator position, const std::type_info &type, void *value, iterator &result);
	bool insert_dyn(iterator position, const std::type_info &type, const void *value, iterator &result);

	template <class InputIterator>
	void insert(iterator position, InputIterator first, InputIterator last)
	{
		data.insert(position, first, last);
		++revision;
	}

	void resize(size_t count);
	void resize(size_t count, const dyn_object &value);
};

class map_t : public collection_base<aux::hybrid_map<dyn_object, dyn_object>>
{
public:
	dyn_object &operator[](const dyn_object &key);
	dyn_object &operator[](dyn_object &&key);
	std::pair<iterator, bool> insert(const dyn_object &key, const dyn_object &value);
	std::pair<iterator, bool> insert(const dyn_object &key, dyn_object &&value);
	std::pair<iterator, bool> insert(dyn_object &&key, const dyn_object &value);
	std::pair<iterator, bool> insert(dyn_object &&key, dyn_object &&value);
	iterator find(const dyn_object &key);
	size_t erase(const dyn_object &key);
	iterator erase(iterator position);
	bool insert_dyn(iterator position, const std::type_info &type, void *value, iterator &result);
	bool insert_dyn(iterator position, const std::type_info &type, const void *value, iterator &result);

	template <class InputIterator>
	void insert(InputIterator first, InputIterator last)
	{
		data.insert(first, last);
		++revision;
	}

	void set_ordered(bool ordered)
	{
		data.set_ordered(ordered);
		++revision;
	}

	bool ordered() const
	{
		return data.is_ordered();
	}
};

class linked_list_t : public collection_base<std::list<std::shared_ptr<dyn_object>>>
{
public:
	dyn_object &operator[](size_t index);
	void push_back(dyn_object &&value);
	void push_back(const dyn_object &value);
	iterator insert(iterator position, dyn_object &&value);
	iterator insert(iterator position, const dyn_object &value);
	bool insert_dyn(iterator position, const std::type_info &type, void *value, iterator &result);
	bool insert_dyn(iterator position, const std::type_info &type, const void *value, iterator &result);

	template <class InputIterator>
	void insert(iterator position, InputIterator first, InputIterator last)
	{
		data.insert(position, first, last);
		++revision;
	}
};

class pool_t : public collection_base<aux::hybrid_pool<dyn_object, 4>>
{
public:
	dyn_object &operator[](size_t index)
	{
		return data[index];
	}
	const dyn_object &operator[](size_t index) const
	{
		return data[index];
	}
	iterator find(size_t index)
	{
		return data.find(index);
	}
	void resize(size_t newsize);
	size_t push_back(dyn_object &&value);
	size_t push_back(const dyn_object &value);

	void set_ordered(bool ordered)
	{
		data.set_ordered(ordered);
		++revision;
	}

	bool ordered() const
	{
		return data.is_ordered();
	}
};

namespace std
{
	template <>
	inline void swap<list_t>(list_t &a, list_t &b) noexcept
	{
		a.swap(b);
	}

	template <>
	inline void swap<map_t>(map_t &a, map_t &b) noexcept
	{
		a.swap(b);
	}

	template <>
	inline void swap<linked_list_t>(linked_list_t &a, linked_list_t &b) noexcept
	{
		a.swap(b);
	}

	template <>
	inline void swap<pool_t>(pool_t &a, pool_t &b) noexcept
	{
		a.swap(b);
	}
}

class dyn_iterator
{
public:
	virtual bool expired() const;
	virtual bool valid() const;
	virtual bool move_next();
	virtual bool move_previous();
	virtual bool set_to_first();
	virtual bool set_to_last();
	virtual bool reset();
	virtual bool erase();
	virtual bool can_reset();
	virtual bool can_insert();
	virtual bool can_erase();
	virtual std::unique_ptr<dyn_iterator> clone() const;
	virtual std::shared_ptr<dyn_iterator> clone_shared() const;
	virtual size_t get_hash() const;
	virtual bool operator==(const dyn_iterator &obj) const;
	int &operator[](size_t index) const;
	virtual ~dyn_iterator() = default;

	template <class Type>
	bool extract(Type &value) const
	{
		return extract_dyn(typeid(Type), reinterpret_cast<void*>(&value));
	}

	template <class Type>
	bool insert(Type &&value)
	{
		return insert_dyn(typeid(Type), &value);
	}

	template <class Type>
	bool insert(const Type &value)
	{
		return insert_dyn(typeid(Type), &value);
	}

protected:
	virtual bool extract_dyn(const std::type_info &type, void *value) const;
	virtual bool insert_dyn(const std::type_info &type, void *value);
	virtual bool insert_dyn(const std::type_info &type, const void *value);
};

template <class Base>
class iterator_impl : public dyn_iterator, public object_pool<dyn_iterator>::ref_container_virtual
{
protected:
	typedef typename Base::iterator iterator;
	typedef typename Base::value_type value_type;
	std::weak_ptr<Base> _source;
	int _revision;
	iterator _position;
	bool _inside;

	virtual std::shared_ptr<Base> lock_same()
	{
		if(auto source = _source.lock())
		{
			if(source->get_revision() == _revision)
			{
				return source;
			}else if(!_inside)
			{
				_position = source->end();
				_revision = source->get_revision();
				return source;
			}
		}
		return nullptr;
	}

	virtual std::shared_ptr<Base> lock_same() const
	{
		if(auto source = _source.lock())
		{
			if(source->get_revision() == _revision)
			{
				return source;
			}
		}
		return nullptr;
	}

public:
	iterator_impl()
	{

	}

	iterator_impl(const std::shared_ptr<Base> source) : iterator_impl(source, source->begin())
	{

	}

	iterator_impl(const std::shared_ptr<Base> source, iterator position) : _source(source), _revision(source->get_revision()), _position(position)
	{
		_inside = position != source->end();
	}

	iterator_impl(const iterator_impl<Base> &iter) : _source(iter._source), _revision(iter._revision), _position(iter._position), _inside(iter._inside)
	{
		
	}

	virtual bool expired() const override
	{
		return _source.expired();
	}

	virtual bool valid() const override
	{
		if(auto source = lock_same())
		{
			return _position != source->end();
		}
		return false;
	}

	virtual bool move_next() override
	{
		if(auto source = lock_same())
		{
			if(_position == source->end())
			{
				return false;
			}else{
				++_position;
			}
			if(_position != source->end())
			{
				return true;
			}
			_inside = false;
		}
		return false;
	}

	virtual bool set_to_first() override
	{
		if(auto source = _source.lock())
		{
			_revision = source->get_revision();
			_position = source->begin();
			if(_position != source->end())
			{
				_inside = true;
				return true;
			}
		}
		return false;
	}

	virtual bool can_reset() override
	{
		return !_source.expired();
	}

	virtual bool reset() override
	{
		if(auto source = _source.lock())
		{
			_revision = source->get_revision();
			_position = source->end();
			_inside = false;
			return true;
		}
		return false;
	}

	virtual size_t get_hash() const override
	{
		if(auto source = _source.lock())
		{
			if(source->get_revision() == _revision)
			{
				return std::hash<decltype(&*_position)>()(&*_position);
			}else if(!_inside)
			{
				return std::hash<Base*>()(source.get());
			}
		}
		return 0;
	}

	virtual bool can_erase() override
	{
		if(auto source = lock_same())
		{
			return _position != source->end();
		}
		return false;
	}

	virtual bool erase() override
	{
		if(auto source = lock_same())
		{
			if(_position != source->end())
			{
				_position = source->erase(_position);
				_revision = source->get_revision();
			}
			return true;
		}
		return false;
	}

	virtual bool operator==(const dyn_iterator &obj) const
	{
		auto other = dynamic_cast<const iterator_impl<Base>*>(&obj);
		if(other != nullptr)
		{
			return !_source.owner_before(other->_source) && !other->_source.owner_before(_source) && _revision == other->_revision && _position == other->_position && _inside == other->_inside;
		}
		return false;
	}

	virtual bool can_insert() override
	{
		if(auto source = lock_same())
		{
			return true;
		}
		return false;
	}

protected:
	virtual bool extract_dyn(const std::type_info &type, void *value) const override
	{
		if(valid())
		{
			if(type == typeid(value_type*))
			{
				*reinterpret_cast<value_type**>(value) = &*_position;
				return true;
			}else if(type == typeid(const value_type*))
			{
				*reinterpret_cast<const value_type**>(value) = &*_position;
				return true;
			}
		}
		return false;
	}

	virtual bool insert_dyn(const std::type_info &type, void *value) override
	{
		if(auto source = lock_same())
		{
			if(source->insert_dyn(_position, type, value, _position))
			{
				_revision = source->get_revision();
				return true;
			}
		}
		return false;
	}

	virtual bool insert_dyn(const std::type_info &type, const void *value) override
	{
		if(auto source = lock_same())
		{
			if(source->insert_dyn(_position, type, value, _position))
			{
				_revision = source->get_revision();
				return true;
			}
		}
		return false;
	}

public:
	virtual dyn_iterator *get() override
	{
		return this;
	}

	virtual const dyn_iterator *get() const override
	{
		return this;
	}
};

class list_iterator_t : public iterator_impl<list_t>
{
public:
	list_iterator_t()
	{

	}

	list_iterator_t(const std::shared_ptr<list_t> source) : iterator_impl(source)
	{

	}

	list_iterator_t(const std::shared_ptr<list_t> source, iterator position) : iterator_impl(source, position)
	{

	}

	list_iterator_t(const list_iterator_t &iter) : iterator_impl(iter)
	{

	}

	virtual bool move_previous() override
	{
		if(auto source = lock_same())
		{
			if(_position == source->end())
			{
				return false;
			}else if(_position == source->begin())
			{
				_position = source->end();
				_inside = false;
				return false;
			}
			--_position;
			return true;
		}
		return false;
	}

	virtual bool set_to_last() override
	{
		if(auto source = _source.lock())
		{
			_revision = source->get_revision();
			_position = source->end();
			if(_position != source->begin())
			{
				--_position;
				return true;
			}
		}
		return false;
	}

	virtual std::unique_ptr<dyn_iterator> clone() const override
	{
		return std::make_unique<list_iterator_t>(*this);
	}

	virtual std::shared_ptr<dyn_iterator> clone_shared() const override
	{
		return std::make_shared<list_iterator_t>(*this);
	}

protected:
	virtual bool extract_dyn(const std::type_info &type, void *value) const override;
};

class map_iterator_t : public iterator_impl<map_t>
{
public:
	map_iterator_t()
	{

	}

	map_iterator_t(const std::shared_ptr<map_t> source) : iterator_impl(source)
	{

	}

	map_iterator_t(const std::shared_ptr<map_t> source, iterator position) : iterator_impl(source, position)
	{

	}

	map_iterator_t(const map_iterator_t &iter) : iterator_impl(iter)
	{

	}
	
	virtual bool move_previous() override
	{
		if(auto source = lock_same())
		{
			if(!source->ordered()) return false;

			if(_position == source->end())
			{
				return false;
			}else if(_position == source->begin())
			{
				_position = source->end();
				_inside = false;
				return false;
			}
			--_position;
			return true;
		}
		return false;
	}

	virtual bool set_to_last() override
	{
		if(auto source = _source.lock())
		{
			if(!source->ordered()) return false;

			_revision = source->get_revision();
			_position = source->end();
			if(_position != source->begin())
			{
				--_position;
				return true;
			}
		}
		return false;
	}

	virtual std::unique_ptr<dyn_iterator> clone() const override
	{
		return std::make_unique<map_iterator_t>(*this);
	}

	virtual std::shared_ptr<dyn_iterator> clone_shared() const override
	{
		return std::make_shared<map_iterator_t>(*this);
	}
};

class linked_list_iterator_t : public dyn_iterator, public object_pool<dyn_iterator>::ref_container_virtual
{
protected:
	typedef typename linked_list_t::iterator iterator;
	typedef typename linked_list_t::value_type value_type;
	std::weak_ptr<linked_list_t> _source;
	iterator _position;
	std::weak_ptr<dyn_object> _current;

	virtual std::shared_ptr<linked_list_t> lock_same();
	virtual std::shared_ptr<linked_list_t> lock_same() const;

public:
	linked_list_iterator_t()
	{

	}

	linked_list_iterator_t(const std::shared_ptr<linked_list_t> source) : linked_list_iterator_t(source, source->begin())
	{

	}

	linked_list_iterator_t(const std::shared_ptr<linked_list_t> source, iterator position) : _source(source), _position(position), _current(position != source->end() ? *position : nullptr)
	{

	}

	linked_list_iterator_t(const linked_list_iterator_t &iter) = default;

	virtual bool expired() const override;
	virtual bool valid() const override;
	virtual bool move_next() override;
	virtual bool move_previous() override;
	virtual bool set_to_first() override;
	virtual bool set_to_last() override;
	virtual bool reset() override;
	virtual size_t get_hash() const override;
	virtual bool erase() override;
	virtual std::unique_ptr<dyn_iterator> clone() const override;
	virtual std::shared_ptr<dyn_iterator> clone_shared() const override;
	virtual bool operator==(const dyn_iterator &obj) const override;

	virtual bool can_reset() override;
	virtual bool can_insert() override;
	virtual bool can_erase() override;

protected:
	virtual bool extract_dyn(const std::type_info &type, void *value) const override;
	virtual bool insert_dyn(const std::type_info &type, void *value) override;
	virtual bool insert_dyn(const std::type_info &type, const void *value) override;

public:
	virtual dyn_iterator *get() override
	{
		return this;
	}

	virtual const dyn_iterator *get() const override
	{
		return this;
	}
};

class handle_t
{
	dyn_object object;
	std::weak_ptr<void> bond;
	bool weak;

public:
	handle_t() = default;

	handle_t(dyn_object &&obj, bool weak = false) : object(std::move(obj)), bond(weak ? object.handle() : (object.acquire(), object.handle())), weak(weak)
	{

	}

	handle_t(dyn_object &&obj, std::weak_ptr<void> &&bond, bool weak = false) : object(std::move(obj)), bond(weak ? std::move(bond) : (object.acquire(), std::move(bond))), weak(weak)
	{

	}

	handle_t(const handle_t &owner, dyn_object &&obj, bool weak = false) : object(std::move(obj)), bond(weak ? owner.bond : (object.acquire(), owner.bond)), weak(weak)
	{

	}

	handle_t(handle_t &&handle) : object(std::move(handle.object)), bond(std::move(handle.bond)), weak(handle.weak)
	{
		handle.weak = true;
	}

	handle_t(const handle_t&) = delete;

	const dyn_object &get() const
	{
		return object;
	}

	const std::weak_ptr<void> &get_bond() const
	{
		return bond;
	}

	int &operator[](size_t index) const
	{
		static int unused;
		return unused;
	}

	handle_t &operator=(handle_t &&handle)
	{
		if(this != &handle)
		{
			object = std::move(handle.object);
			bond = std::move(handle.bond);
			weak = handle.weak;
		}
		return *this;
	}

	bool linked() const
	{
		return !bond.expired();
	}

	void release()
	{
		if(!weak && alive())
		{
			object.release();
		}
	}

	bool is_weak() const
	{
		return weak;
	}

	handle_t &operator=(const handle_t&) = delete;

	bool alive() const
	{
		return !bond.expired() || (!bond.owner_before(std::weak_ptr<void>{}) && !std::weak_ptr<void>{}.owner_before(bond));
	}

	bool operator==(const handle_t &obj)
	{
		return object == obj.object && (!bond.owner_before(obj.bond) && !obj.bond.owner_before(bond));
	}

	~handle_t()
	{
		release();
	}
};

extern aux::shared_id_set_pool<list_t> list_pool;
extern aux::shared_id_set_pool<map_t> map_pool;
extern aux::shared_id_set_pool<linked_list_t> linked_list_pool;
extern aux::shared_id_set_pool<pool_t> pool_pool;
extern object_pool<dyn_iterator> iter_pool;
extern object_pool<handle_t> handle_pool;

#endif
