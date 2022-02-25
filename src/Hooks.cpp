#include "Hooks.h"

namespace Hooks
{
	void ScrollWheelFix()
	{
		auto moduleBase = REL::Module::get().base();
		auto injectionPoint = moduleBase + 0xc119d8; //Function c11600 67242

		struct OrigCode : Xbyak::CodeGenerator
		{
			OrigCode()
			{
				mov(eax, qword[rax + 0x118]);
			}
		};
		OrigCode orig;
		orig.ready();

		struct DeleteBit6 : Xbyak::CodeGenerator
		{
			DeleteBit6()
			{
				and_(cl, 0xdf); //delete bit #6
			}
		};
		DeleteBit6 db6;
		db6.ready();

		static SKSE::Trampoline injectedCode;
		//auto injectedCodeSize = orig.getSize() + db6.getSize() + 5; //branch5
		auto injectedCodeSize = 32; //branch5 allocates too much mem
		injectedCode.create(injectedCodeSize, (void*)injectionPoint);

		injectedCode.allocate(orig);
		injectedCode.allocate(db6);

		//erase the original instruction with nops
		std::vector<std::uint8_t> nops(orig.getSize());
		std::fill(nops.begin(), nops.end(), (std::uint8_t)0x90);
		REL::safe_write<std::uint8_t>(injectionPoint, nops);

		//writes:
		//jmp from injectionPoint -> start_InjectedCode
		//jmp from end_InjectedCode -> injectionPoint+org.getSize()
		injectedCode.write_branch<5>(injectionPoint, injectionPoint+orig.getSize());

		logger::info("fixed scroll wheel zoom during object animations!");
		
	}

	void Install()
	{
		ScrollWheelFix();
	}
}
