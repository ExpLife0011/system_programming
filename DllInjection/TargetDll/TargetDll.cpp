#include <stdio.h>
#include <windows.h>

struct sc_data_t {
	char libName[16];
	void* ploadLibrary;
	UINT32 a, b, c;
	UINT32 reserved;
};

void* FindShellcodeData()
{
	CONTEXT                       Context;
	KNONVOLATILE_CONTEXT_POINTERS NvContext;
	UNWIND_HISTORY_TABLE          UnwindHistoryTable;
	PRUNTIME_FUNCTION             RuntimeFunction;
	PVOID                         HandlerData;
	ULONG64                       EstablisherFrame;
	ULONG64                       ImageBase;
	ULONG64						  pData;

	// First, we'll get the caller's context.
	RtlCaptureContext(&Context);


	// Initialize the (optional) unwind history table.
	RtlZeroMemory(
		&UnwindHistoryTable,
		sizeof(UNWIND_HISTORY_TABLE));

	// This unwind loop intentionally skips the first call frame, as it shall
	// correspond to the call to StackTrace64, which we aren't interested in.
	for (ULONG Frame = 0;; Frame++) {
		// Try to look up unwind metadata for the current function.
		RuntimeFunction = RtlLookupFunctionEntry(
			Context.Rip,
			&ImageBase,
			&UnwindHistoryTable);

		RtlZeroMemory(
			&NvContext,
			sizeof(KNONVOLATILE_CONTEXT_POINTERS));

		if (!RuntimeFunction) {
			// If we don't have a RUNTIME_FUNCTION, then we've encountered
			// a leaf function.  Adjust the stack approprately.
			Context.Rip  = (ULONG64)(*(PULONG64)Context.Rsp);
			Context.Rsp += 8;
		} else {
			// Otherwise, call upon RtlVirtualUnwind to execute the unwind for us.
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

		// If we reach an RIP of zero, this means that we've walked off the end
		// of the call stack and are done.
		if (!Context.Rip)
			break;

		// Display the context.  Note that we don't bother showing the XMM
		// context, although we have the nonvolatile portion of it.
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
			Context.Rsp);

		pData = Context.Rsp;
		printf("\n");
	}
	return (char*)pData + 0x28;
}

BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved)
{
	if (fdwReason != DLL_PROCESS_ATTACH)
		return TRUE;

	sc_data_t* pData;
	pData = (sc_data_t*) FindShellcodeData();
	pData->c = pData->a + pData->b;
	printf("a = %d, b = %d, c = %d, s = %s\n", pData->a, pData->b, pData->c, pData->libName);
	printf("data: 0x%p, 0x%p\n", pData, *pData);
	return TRUE;
}