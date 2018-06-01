#include "reset.h"
#include "fixes/linux.h"
#include <cstring>

namespace amx
{
	reset::reset(AMX* amx, bool context) : amx(amx::load(amx)), cip(amx->cip), frm(amx->frm), pri(amx->pri), alt(amx->alt), hea(amx->hea), reset_hea(amx->reset_hea), stk(amx->stk), reset_stk(amx->reset_stk)
	{
		if(context)
		{
			amx::object owner;
			this->context = std::move(amx::get_context(amx, owner));
		}

		unsigned char *dat, *h, *s;

		auto amxhdr = (AMX_HEADER*)amx->base;
		dat = amx->base + amxhdr->dat;
		h = amx->base + amxhdr->hea;
		s = dat + stk;

		size_t heap_size = hea - (amxhdr->hea - amxhdr->dat);
		heap = std::make_unique<unsigned char[]>(heap_size);
		std::memcpy(heap.get(), h, heap_size);

		size_t stack_size = amxhdr->stp - amxhdr->dat - stk;
		stack = std::make_unique<unsigned char[]>(stack_size);
		std::memcpy(stack.get(), s, stack_size);
	}

	bool reset::restore()
	{
		if(!restore_no_context()) return false;
		auto obj = amx.lock();
		if(!obj || !obj->valid()) return false;
		auto amx = obj->get();
		amx::restore(amx, std::move(context));
		return true;
	}

	bool reset::restore_no_context() const
	{
		auto obj = amx.lock();
		if(!obj || !obj->valid()) return false;

		auto amx = obj->get();

		unsigned char *dat, *h, *s;

		auto amxhdr = (AMX_HEADER*)amx->base;
		dat = amx->base + amxhdr->dat;
		h = amx->base + amxhdr->hea;
		s = dat + stk;

		amx->cip = cip;
		amx->frm = frm;
		amx->pri = pri;
		amx->alt = alt;
		amx->hea = hea;
		amx->reset_hea = hea;
		amx->stk = stk;
		amx->reset_stk = stk;

		size_t heap_size = hea - (amxhdr->hea - amxhdr->dat);
		std::memcpy(h, heap.get(), heap_size);

		size_t stack_size = amxhdr->stp - amxhdr->dat - stk;
		std::memcpy(s, stack.get(), stack_size);

		return true;
	}

	reset::reset(reset &&obj) : context(std::move(obj.context)), amx(obj.amx), cip(obj.cip), frm(obj.frm), pri(obj.pri), alt(obj.alt), hea(obj.hea), reset_hea(obj.reset_hea), heap(std::move(obj.heap)), stk(obj.stk), reset_stk(obj.reset_stk), stack(std::move(obj.stack))
	{

	}

	reset &reset::operator=(reset &&obj)
	{
		if(this == &obj) return *this;

		context = std::move(obj.context);
		amx = obj.amx;
		cip = obj.cip, frm = obj.frm, pri = obj.pri, alt = obj.alt;
		stk = obj.stk, reset_stk = obj.reset_stk;
		hea = obj.hea, reset_hea = obj.reset_hea;
		heap = std::move(obj.heap);
		stack = std::move(obj.stack);
		return *this;
	}
}