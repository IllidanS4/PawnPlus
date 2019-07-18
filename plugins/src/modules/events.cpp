#include "events.h"
#include "amxinfo.h"
#include "hooks.h"
#include "main.h"
#include "errors.h"
#include "objects/stored_param.h"
#include "utils/optional.h"

#include <cstring>
#include <memory>
#include <vector>
#include <unordered_map>

class event_info
{
	cell flags;
	std::vector<stored_param> arg_values;
	std::string handler;
	aux::optional<int> index;
	bool handler_index(AMX *amx, int &index);

public:
	event_info(cell flags, AMX *amx, const char *function, const char *format, const cell *args, size_t numargs);
	bool invoke(AMX *amx, cell *retval, cell id);
};

class callback_info
{
	expression_ptr action;

public:
	std::string name;

	callback_info(expression_ptr &&action, const std::string &name) : action(std::move(action)), name(name)
	{

	}

	void invoke(AMX *amx, cell *retval);
};

class amx_info : public amx::extra
{
public:
	std::vector<std::vector<std::unique_ptr<event_info>>> callback_handlers;
	std::unordered_map<int, std::vector<std::unique_ptr<event_info>>> callback_handlers_negative;
	std::unordered_map<cell, int> handler_ids;

	std::vector<callback_info> custom_callbacks;
	std::unordered_map<std::string, int> custom_callbacks_map;
	int name_length = 0;

	amx_info(AMX *amx) : amx::extra(amx)
	{

	}
};

amx_info &get_info(AMX *amx, amx::object &obj)
{
	obj = amx::load_lock(amx);
	return obj->get_extra<amx_info>();
}

namespace events
{
	int register_callback(const char *callback, cell flags, AMX *amx, const char *function, const char *format, const cell *params, int numargs)
	{
		amx::object obj;
		int index;
		if(amx_FindPublicSafe(amx, callback, &index) != AMX_ERR_NONE)
		{
			amx_FormalError(errors::func_not_found, "public", callback);
		}
		auto &info = get_info(amx, obj);
		std::vector<std::unique_ptr<event_info>> *list;
		if(index >= 0)
		{
			auto &callback_handlers = info.callback_handlers;
			if(static_cast<size_t>(index) >= callback_handlers.size())
			{
				callback_handlers.resize(index + 1);
			}
			list = &callback_handlers[index];
		}else{
			// a negative index may be provided by some plugins
			list = &info.callback_handlers_negative[index];
		}
		auto ptr = std::unique_ptr<event_info>(new event_info(flags, amx, function, format, params, numargs));
		cell id = reinterpret_cast<cell>(ptr.get());
		list->push_back(std::move(ptr));
		info.handler_ids[id] = index;
		return id;
	}

	bool remove_callback(AMX *amx, cell id)
	{
		amx::object obj;
		auto &info = get_info(amx, obj);
		auto &handler_ids = info.handler_ids;
		auto ptr = reinterpret_cast<event_info*>(id);
		auto hit = handler_ids.find(id);
		if(hit != handler_ids.end())
		{
			int index = hit->second;
			std::vector<std::unique_ptr<event_info>> *list;
			if(index >= 0)
			{
				list = &info.callback_handlers[index];
			}else{
				list = &info.callback_handlers_negative[index];
			}
			for(auto it = list->begin(); it != list->end(); it++)
			{
				if(it->get() == ptr)
				{
					list->erase(it);
					handler_ids.erase(hit);
					return true;
				}
			}
			handler_ids.erase(hit);
		}
		return false;
	}

	bool invoke_callbacks(AMX *amx, int index, cell *retval)
	{
		amx::object obj;
		auto &info = get_info(amx, obj);
		std::vector<std::unique_ptr<event_info>> *list = nullptr;;
		if(index >= 0)
		{
			auto &callback_handlers = info.callback_handlers;
			if(static_cast<size_t>(index) < callback_handlers.size())
			{
				list = &callback_handlers[index];
			}
		}else{
			auto &callback_handlers_negative = info.callback_handlers_negative;
			auto it = callback_handlers_negative.find(index);
			if(it != callback_handlers_negative.end())
			{
				list = &it->second;
			}
		}
		if(list)
		{
			for(auto &handler : *list)
			{
				if(handler)
				{
					if(handler->invoke(amx, retval, reinterpret_cast<cell>(handler.get())))
					{
						return true;
					}
				}
			}
		}

		int number;
		amx_NumPublicsOrig(amx, &number);
		index -= number;

		if(index >= 0 && static_cast<size_t>(index) < info.custom_callbacks.size())
		{
			info.custom_callbacks[index].invoke(amx, retval);
			return true;
		}

		return false;
	}
}

event_info::event_info(cell flags, AMX *amx, const char *function, const char *format, const cell *args, size_t numargs)
	: flags(flags), handler(function)
{
	if(format != nullptr)
	{
		size_t argi = -1;
		size_t len = std::strlen(format);
		for(size_t i = 0; i < len; i++)
		{
			arg_values.push_back(stored_param::create(amx, format[i], args, argi, numargs));
		}
	}
}

bool event_info::handler_index(AMX *amx, int &index)
{
	if(this->index.has_value())
	{
		index = this->index.value();

		int len;
		amx_NameLength(amx, &len);
		char *funcname = static_cast<char*>(alloca(len + 1));

		if(amx_GetPublic(amx, index, funcname) == AMX_ERR_NONE && !std::strcmp(handler.c_str(), funcname))
		{
			return true;
		}else if(amx_FindPublicSafe(amx, handler.c_str(), &index) == AMX_ERR_NONE)
		{
			this->index = index;
			return true;
		}
	}else if(amx_FindPublicSafe(amx, handler.c_str(), &index) == AMX_ERR_NONE)
	{
		this->index = index;
		return true;
	}
	logwarn(amx, "[PawnPlus] Callback handler %s was not found.", handler.c_str());
	return false;
}

bool event_info::invoke(AMX *amx, cell *retval, cell id)
{
	int index;
	if(!handler_index(amx, index)) return false;

	int params = amx->paramcount;
	cell stk = amx->stk;

	cell flags = this->flags;

	std::vector<cell> oldargs;
	if(!(flags & 2))
	{
		cell *stk = reinterpret_cast<cell*>(amx_GetData(amx) + amx->stk);
		oldargs = {stk, stk + amx->paramcount};
	}

	cell heap, *heap_addr;
	amx_Allot(amx, 0, &heap, &heap_addr);

	cell *retarg = nullptr;
	if(flags & 1)
	{
		cell retaddr;
		amx_Allot(amx, 1, &retaddr, &retarg);
		amx_Push(amx, retaddr);
		if(retval)
		{
			*retarg = *retval;
		}
	}

	for(auto it = arg_values.rbegin(); it != arg_values.rend(); it++)
	{
		it->push(amx, id);
	}

	cell handled = 0;
	int err = amx_Exec(amx, &handled, index);

	amx_Release(amx, heap);

	if(!handled)
	{
		if(flags & 2)
		{
			amx->paramcount = params;
			amx->stk = stk;
		}else for(auto it = oldargs.rbegin(); it != oldargs.rend(); it++)
		{
			amx_Push(amx, *it);
		}
	}

	if(err) amx_RaiseError(amx, err);

	if(handled)
	{
		if(retval)
		{
			if(retarg)
			{
				*retval = *retarg;
			}else{
				*retval = handled;
			}
		}
		return true;
	}
	return false;
}

int events::new_callback(const char *callback, AMX *amx, expression_ptr &&default_action)
{
	int index;
	if(amx_FindPublicSafe(amx, callback, &index) == AMX_ERR_NONE)
	{
		return index;
	}
	amx::object obj;
	auto &info = get_info(amx, obj);
	std::string name(callback);
	if(name.size() > static_cast<size_t>(info.name_length))
	{
		info.name_length = name.size();
	}
	info.custom_callbacks.emplace_back(std::move(default_action), name);
	amx_NumPublics(amx, &index);
	info.custom_callbacks_map[std::move(name)] = index - 1;
	return index - 1;
}

int events::num_callbacks(AMX *amx)
{
	amx::object obj;
	auto &info = get_info(amx, obj);
	return info.custom_callbacks.size();
}

int events::find_callback(AMX *amx, const char *callback)
{
	amx::object obj;
	auto &info = get_info(amx, obj);
	auto it = info.custom_callbacks_map.find(callback);
	if(it != info.custom_callbacks_map.end())
	{
		return it->second;
	}
	return -1;
}

const char *events::callback_name(AMX *amx, int index)
{
	int number;
	amx_NumPublicsOrig(amx, &number);
	index -= number;
	if(index < 0)
	{
		return nullptr;
	}
	amx::object obj;
	auto &info = get_info(amx, obj);
	if(static_cast<size_t>(index) >= info.custom_callbacks.size())
	{
		return nullptr;
	}
	return info.custom_callbacks[index].name.c_str();
}

void events::name_length(AMX *amx, int &length)
{
	amx::object obj;
	auto &info = get_info(amx, obj);
	if(info.name_length > length)
	{
		length = info.name_length;
	}
}

void callback_info::invoke(AMX *amx, cell *retval)
{
	int params = amx->paramcount;
	amx->paramcount = 0;

	cell *stk = reinterpret_cast<cell*>(amx_GetData(amx) + amx->stk);
	amx->stk += params * sizeof(cell);

	if(!action)
	{
		amx->error = AMX_ERR_NOTFOUND;
		return;
	}

	expression::args_type args;
	std::vector<dyn_object> args_cont;
	for(int i = 0; i < params; i++)
	{
		args_cont.emplace_back(stk[i], tags::find_tag(tags::tag_cell));
		args.push_back(std::cref(args_cont.back()));
	}

	amx::guard guard(amx);
	try{
		auto result = action->execute(args, expression::exec_info(amx));
		if(!result.is_null() && result.array_size() > 0)
		{
			*retval = result.get_cell(0);
		}else{
			*retval = 0;
		}
	}catch(const errors::native_error &err)
	{
		logprintf("[PawnPlus] Unhandled error in custom callback %s: %s", name.c_str(), err.message.c_str());
	}catch(const errors::amx_error &err)
	{
		amx->error = err.code;
	}
}
