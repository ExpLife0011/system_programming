#include <stdio.h>
#include <windows.h>

#include "WinStructures.h"

extern "C" void funcAsm();

void func3()
{
	printf("func3 enter \n");
	
	CONTEXT                       Context;
	KNONVOLATILE_CONTEXT_POINTERS NvContext;
	UNWIND_HISTORY_TABLE          UnwindHistoryTable;
	PRUNTIME_FUNCTION             RuntimeFunction;
	PVOID                         HandlerData;
	ULONG64                       EstablisherFrame;
	ULONG64                       ImageBase;

	//
	// First, we'll get the caller's context.
	//

	RtlCaptureContext(&Context);

	//
	// Initialize the (optional) unwind history table.
	//

	RtlZeroMemory(
		&UnwindHistoryTable,
		sizeof(UNWIND_HISTORY_TABLE));

	//UnwindHistoryTable.Unwind = TRUE; // fck

	//
	// This unwind loop intentionally skips the first call frame, as it shall
	// correspond to the call to StackTrace64, which we aren't interested in.
	//

	for (ULONG Frame = 0;
		 ;
		 Frame++)
	{
		//
		// Try to look up unwind metadata for the current function.
		//

		RuntimeFunction = RtlLookupFunctionEntry(
			Context.Rip,
			&ImageBase,
			&UnwindHistoryTable
			);

		RtlZeroMemory(
			&NvContext,
			sizeof(KNONVOLATILE_CONTEXT_POINTERS));

		if (!RuntimeFunction)
		{
			//
			// If we don't have a RUNTIME_FUNCTION, then we've encountered
			// a leaf function.  Adjust the stack approprately.
			//

			Context.Rip  = (ULONG64)(*(PULONG64)Context.Rsp);
			Context.Rsp += 8;
		}
		else
		{
			//
			// Otherwise, call upon RtlVirtualUnwind to execute the unwind for
			// us.
			//

			RtlVirtualUnwind(
				UNW_FLAG_NHANDLER,
				ImageBase,
				Context.Rip,
				RuntimeFunction,
				&Context,
				&HandlerData,
				&EstablisherFrame,
				&NvContext);
		}

		//
		// If we reach an RIP of zero, this means that we've walked off the end
		// of the call stack and are done.
		//

		if (!Context.Rip)
			break;

		//
		// Display the context.  Note that we don't bother showing the XMM
		// context, although we have the nonvolatile portion of it.
		//

		printf(
			"FRAME %02x: Rip=%p Rsp=%p Rbp=%p\n",
			Frame,
			Context.Rip,
			Context.Rsp,
			Context.Rbp);
		printf(
			"r12=%p r13=%p r14=%p\n"
			"rdi=%p rsi=%p rbx=%p\n"
			"rbp=%p rsp=%p\n",
			Context.R12,
			Context.R13,
			Context.R14,
			Context.Rdi,
			Context.Rsi,
			Context.Rbx,
			Context.Rbp,
			Context.Rsp
			);

		static const CHAR* RegNames[ 16 ] =
		{ "Rax", "Rcx", "Rdx", "Rbx", "Rsp", "Rbp", "Rsi", "Rdi", "R8", "R9",
		  "R10", "R11", "R12", "R13", "R14", "R15" };

		//
		// If we have stack-based register stores, then display them here.
		//

		for (ULONG i = 0;
			 i < 16;
			 i++)
		{
			if (NvContext.IntegerContext[ i ])
			{
				printf(
					" -> Saved register '%s' on stack at %p (=> %p)\n",
					RegNames[ i ],
					NvContext.IntegerContext[ i ],
					*NvContext.IntegerContext[ i ]);
			}
		}

		printf("\n");
	}

	printf("func3 exit \n");
}

void func2()
{
	printf("func2 enter \n");
	func3();
	printf("func2 exit \n");
}

void func1()
{
	printf("func1 enter \n");
	func2();
	printf("func1 exit \n");
}

int DllMain()
{
	printf("Main enter \n");
	funcAsm();
	func1();
	printf("Main exit \n");
	getchar();
	return 0;
}