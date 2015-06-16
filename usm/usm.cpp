#include "stdafx.h"

HANDLE stdout_handle;

PUMS_COMPLETION_LIST completionList = 0;
CHAR* reasons[3] = {"Startup", "ThreadBlocked", "ThreadYield"};
SLIST_HEADER contextList;
SLIST_HEADER contextSuspendedList;
SLIST_HEADER contextFreeList;

#define writeFileCHARS(dst, charsWritten)

typedef struct _SINGLE_LIST_CONTEXT_ENTRY {
	SLIST_ENTRY entry;
	PUMS_CONTEXT context;
} SINGLE_LIST_CONTEXT_ENTRY, *PSINGLE_LIST_CONTEXT_ENTRY;

BOOL isUsmThreadSuspended(PUMS_CONTEXT UmsThread){
	BOOLEAN UmsThreadInformation = 0;
	ULONG ReturnLength = 0;
	BOOL success = QueryUmsThreadInformation(UmsThread, UmsThreadIsSuspended, &UmsThreadInformation, sizeof(UmsThreadInformation), &ReturnLength);
	if(!success){
		DWORD le = GetLastError();
		DebugBreak();
	}
	return UmsThreadInformation;
}

BOOL isUsmThreadTerminated(PUMS_CONTEXT UmsThread){
	BOOLEAN UmsThreadInformation = 0;
	ULONG ReturnLength = 0;
	BOOL success = QueryUmsThreadInformation(UmsThread, UmsThreadIsTerminated, &UmsThreadInformation, sizeof(UmsThreadInformation), &ReturnLength);
	if(!success){
		DWORD le = GetLastError();
		DebugBreak();
	}
	return UmsThreadInformation;
}

DWORD WINAPI threadProc(__in  LPVOID lpParameter){
/*
	CHAR dst[1000];
	size_t charsWritten = sprintf_s(dst, "threadProc %d\n", lpParameter);
	OutputDebugString(dst);
	DWORD bytesWritten = 0;
	BOOL ret = WriteFile(stdout_handle, dst, charsWritten * sizeof(TCHAR), &bytesWritten, NULL);
*/
	writeFileFormatted1("threadProc %d\n", lpParameter);
	return 0;
}

VOID NTAPI entryPoint(__in RTL_UMS_SCHEDULER_REASON Reason, __in ULONG_PTR ActivationPayload, __in PVOID SchedulerParam){
	BOOL success;
	CHAR dst[1000];
	if(UmsSchedulerStartup == Reason){
		SIZE_T Size = 0;
		success = InitializeProcThreadAttributeList(NULL, 1, 0, &Size);
		LPPROC_THREAD_ATTRIBUTE_LIST atList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(Size);
		for(int n = 0; n<50; n++){
			InterlockedPushEntrySList(&contextFreeList, (PSLIST_ENTRY)malloc(sizeof(SINGLE_LIST_CONTEXT_ENTRY)));
			PUMS_CONTEXT context = 0;
			success = CreateUmsThreadContext(&context);
			//printf("CreateThread %d, %x\n", n, context);
			UMS_CREATE_THREAD_ATTRIBUTES createThreadAttributes = {UMS_VERSION, context, completionList};
			SecureZeroMemory(atList, Size);
			success = InitializeProcThreadAttributeList(atList, 1, 0, &Size);
			success = UpdateProcThreadAttribute(atList, 0, PROC_THREAD_ATTRIBUTE_UMS_THREAD, &createThreadAttributes, sizeof(UMS_CREATE_THREAD_ATTRIBUTES), NULL, NULL);
			HANDLE threadHandle = CreateRemoteThreadEx(GetCurrentProcess(), NULL, 0, threadProc, (LPVOID)n, 0, atList, NULL);
			if(!threadHandle){
				DWORD le = GetLastError();
				printf("%d", le);
			}
			CloseHandle(threadHandle);
			DeleteProcThreadAttributeList(atList);
		}
		free(atList);
	}

	size_t charsWritten = sprintf_s(dst, "entryPoint %s %x %x\n", reasons[Reason], ActivationPayload, SchedulerParam);
	writeFileCHARS(dst, charsWritten);
	PUMS_CONTEXT umsthreadlist = 0, nextThread = 0;
	//	The application-defined UMS scheduler entry point function associated with a UMS completion list.
	for(;;){
		PSINGLE_LIST_CONTEXT_ENTRY contextEntry;
		BOOL listNotEmpty = FALSE;

		charsWritten = sprintf_s(dst, "DequeueUmsCompletionListItems");
		writeFileCHARS(dst, charsWritten);
		success = DequeueUmsCompletionListItems(completionList, 0, &umsthreadlist);//	Retrieves UMS worker threads from the specified UMS completion list.
		charsWritten = sprintf_s(dst, "%s", success?"SUCCESS\n":"!SUCCESS\n");
		writeFileCHARS(dst, charsWritten);
		nextThread = umsthreadlist;
		while(nextThread){
			//OutputDebugString("GetNextUmsListItem ");
			PSINGLE_LIST_CONTEXT_ENTRY contextEntry = (PSINGLE_LIST_CONTEXT_ENTRY)InterlockedPopEntrySList(&contextFreeList);
			contextEntry->context = nextThread;
			InterlockedPushEntrySList(&contextList, &contextEntry->entry);
			CHAR dst[1000];
			charsWritten = sprintf_s(dst, "contextEntry pushed %x\n", nextThread);
			writeFileCHARS(dst, charsWritten);
			nextThread = GetNextUmsListItem(nextThread);//	Returns the next UMS thread context in a list of UMS thread contexts.
			//OutputDebugString(nextThread?"nextThread\n":"!nextThread\n");
		};


		do{
			while(contextEntry = (PSINGLE_LIST_CONTEXT_ENTRY)InterlockedPopEntrySList(&contextList)){
				nextThread = contextEntry->context;
				charsWritten = sprintf_s(dst, "contextEntry popped %x\n", nextThread);
				writeFileCHARS(dst, charsWritten);
				if(isUsmThreadSuspended(nextThread)){
					charsWritten = sprintf_s(dst, "context suspended %x\n", nextThread);
					writeFileCHARS(dst, charsWritten);
					InterlockedPushEntrySList(&contextSuspendedList, &contextEntry->entry);
				}else if(isUsmThreadTerminated(nextThread)){
					charsWritten = sprintf_s(dst, "context terminated %x\n", nextThread);
					writeFileCHARS(dst, charsWritten);
					DeleteUmsThreadContext(contextEntry->context);
					free(contextEntry);
				}else{
					InterlockedPushEntrySList(&contextFreeList, &contextEntry->entry);
					for(;;){
						charsWritten = sprintf_s(dst, "ExecuteUmsThread %x\n", nextThread);
						writeFileCHARS(dst, charsWritten);
						success = ExecuteUmsThread(nextThread);
						if(!success){
							DWORD le = GetLastError();
							if(ERROR_RETRY == le){
								charsWritten = sprintf_s(dst, "ERROR_RETRY\n");
								writeFileCHARS(dst, charsWritten);
								continue;
							}else if(ERROR_INVALID_PARAMETER == le){
								charsWritten = sprintf_s(dst, "ERROR_INVALID_PARAMETER\n");
								writeFileCHARS(dst, charsWritten);
								if(isUsmThreadSuspended(nextThread)){
									charsWritten = sprintf_s(dst, "context suspended LATE\n");
									writeFileCHARS(dst, charsWritten);
								}else if(isUsmThreadTerminated(nextThread)){
									charsWritten = sprintf_s(dst, "context terminated LATE\n");
									writeFileCHARS(dst, charsWritten);
									DeleteUmsThreadContext(contextEntry->context);
									free(contextEntry);
								}else{
									charsWritten = sprintf_s(dst, "context error skip\n");
									writeFileCHARS(dst, charsWritten);
								}
								break;
							}else{
								charsWritten = sprintf_s(dst, "ExecuteUmsThread error %d\n", le);
								writeFileCHARS(dst, charsWritten);
								//	success = DeleteUmsThreadContext(context);
								break;
							}
						}
					}
				}
			}
			while(contextEntry = (PSINGLE_LIST_CONTEXT_ENTRY)InterlockedPopEntrySList(&contextSuspendedList)){
				charsWritten = sprintf_s(dst, "TrySuspended\n");
				writeFileCHARS(dst, charsWritten);
				InterlockedPushEntrySList(&contextList, &contextEntry->entry);
				listNotEmpty = TRUE;
			}
		}while(listNotEmpty);

		charsWritten = sprintf_s(dst, "DequeueUmsCompletionListItems");
		writeFileCHARS(dst, charsWritten);
		success = DequeueUmsCompletionListItems(completionList, INFINITE, &umsthreadlist);//	Retrieves UMS worker threads from the specified UMS completion list.
		charsWritten = sprintf_s(dst, "%s", success?"SUCCESS\n":"!SUCCESS\n");
		writeFileCHARS(dst, charsWritten);
		nextThread = umsthreadlist;
		do{
			//OutputDebugString("GetNextUmsListItem ");
			PSINGLE_LIST_CONTEXT_ENTRY contextEntry = (PSINGLE_LIST_CONTEXT_ENTRY)InterlockedPopEntrySList(&contextFreeList);
			contextEntry->context = nextThread;
			InterlockedPushEntrySList(&contextList, &contextEntry->entry);
			CHAR dst[1000];
			charsWritten = sprintf_s(dst, "contextEntry pushed %x\n", nextThread);
			writeFileCHARS(dst, charsWritten);
			nextThread = GetNextUmsListItem(nextThread);//	Returns the next UMS thread context in a list of UMS thread contexts.
			//OutputDebugString(nextThread?"nextThread\n":"!nextThread\n");
		}while(nextThread);

	}
	return;
}

int _tmain(int argc, _TCHAR* argv[]) {
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	BOOL success = false;
	InitializeSListHead(&contextList);
	InitializeSListHead(&contextSuspendedList);
	InitializeSListHead(&contextFreeList);

	success = CreateUmsCompletionList(&completionList);//	Creates a UMS completion list.
	UMS_SCHEDULER_STARTUP_INFO startupInfo = {UMS_VERSION, completionList, entryPoint, 0};

	/*
	ExecuteUmsThread;//	Runs the specified UMS worker thread.
	GetCurrentUmsThread;//	Returns the UMS thread context of the calling UMS thread.
	GetUmsCompletionListEvent;//	Retrieves a handle to the event associated with the specified UMS completion list.
	//GetUmsSystemThreadInformation;//	Queries whether the specified thread is a UMS scheduler thread, a UMS worker thread, or a non-UMS thread.
	QueryUmsThreadInformation;//	Retrieves information about the specified UMS worker thread.
	SetUmsThreadInformation;//	Sets application-specific context information for the specified UMS worker thread.
	UmsThreadYield;//	Yields control to the UMS scheduler thread on which the calling UMS worker thread is running.
	*/

	success = EnterUmsSchedulingMode(&startupInfo);//	Converts the calling thread into a UMS scheduler thread.
	
	success = DeleteUmsCompletionList(completionList);//	Deletes the specified UMS completion list. The list must be empty.

	return 0;
}