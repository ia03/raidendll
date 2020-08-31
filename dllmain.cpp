// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "detours.h"
#include "dllmain.h"
#include <string>
#include <sstream>



void write_pipe(const char* data)
{
	int bytes_written = 0;
	WriteFile(write_file_handle, data, strlen(data), (LPDWORD)&bytes_written, NULL);
}

void read_string(char* output) {
	ULONG read = 0;
	int index = 0;
	do {
		ReadFile(read_file_handle, output + index++, 1, &read, NULL);
	} while (read > 0 && *(output + index - 1) != 0);
}


int sig_scan(std::string sig)
{
	InitializeSigScan(GetCurrentProcessId(), "GameAssembly.dll");
	int result = SigScan(sig.c_str(), 0);
	FinalizeSigScan();
	return result;
}


int hook_get_player_by_id(int a1, char id)
{
	get_player_info original_get_player_by_id = (get_player_info)getpinfo_addr;
	last_a1 = a1;
	int result = original_get_player_by_id(a1, id);
	player_array = *(DWORD *)(a1 + 0x24);  // Offset is 0x24.
	return result;
}


typedef int(*get_player_count)(int a1, int a2);
int hook_get_player_count(int a1, int a2)
{
	get_player_count original_get_player_count = (get_player_count)get_pcount_addr;
	int result = original_get_player_count(a1, a2);
	player_count = result;
	return result;
}

typedef int(*late_update)(int a1);
int hook_late_update(int a1)
{
	late_update original_late_update = (late_update)late_update_addr;
	if (frame_count == 15)
	{
		check_pipe();
		send_impostors();
		frame_count = 0;
	}
	else
	{
		frame_count++;
	}
	return original_late_update(a1);
}

void setup_pipe()
{
	read_file_handle = CreateFileW(TEXT("\\\\.\\pipe\\raidencolor"),
		GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, 0, NULL);
	write_file_handle = CreateFileW(TEXT("\\\\.\\pipe\\raidenreveal"),
		GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, 0, NULL);
	COMMTIMEOUTS commtimeouts;
	commtimeouts.ReadTotalTimeoutMultiplier = 0;
	commtimeouts.ReadIntervalTimeout = 0;
	commtimeouts.ReadTotalTimeoutConstant = 0;
	commtimeouts.WriteTotalTimeoutMultiplier = 0;
	commtimeouts.WriteTotalTimeoutConstant = 0;
	SetCommTimeouts(write_file_handle, &commtimeouts);
	
}


typedef int(*getpinfo)(int a1, unsigned int index);
char *get_player_addr(int index)
{
	return (char *)((getpinfo)getpinfo_addr)(last_a1, index);
}

char get_color(char *player_address)
{
	return *((char *)player_address + 0x10);
}

char *get_player_by_color(int color_id)
{
	if (!player_array)
	{
		return nullptr;
	}
	for (int i = 0; i < player_count; i++)
	{
		char *address = get_player_addr(i);
		char color = get_color(address);
		if ((int)color == color_id)
		{
			return address;
		}
	}
	return nullptr;
}

char *get_player_by_color(std::stringstream &string_stream)
{
	int color_id;
	string_stream >> color_id;
	return get_player_by_color(color_id);
}

int task_count(DWORD *player_info)
{
	DWORD list_ptr = *(player_info + 9);
	if (list_ptr)
	{
		int *result = (int *)(list_ptr + 0xC);
		return *result;
	}
	else
	{
		return 0;
	}
}

void complete_tasks(std::stringstream &string_stream)
{
	completetask original_completetask = (completetask)completetask_addr;
	getpcontrol original_getpcontrol = (getpcontrol)getpcontrol_addr;
	char *player_info = get_player_by_color(string_stream);
	if (!player_info)
	{
		return;
	}
	int count = task_count((DWORD *)player_info);
	char *player_control = original_getpcontrol(player_info);
	for (int i = 0; i < count; i++)
	{
		original_completetask(player_control, i);
	}

}

void send_impostors()
{
	if (!player_array)
	{
		return;
	}
	get_player_info original_get_player_by_id = (get_player_info)getpinfo_addr;
	std::string output = "i ";
	for (int i = 0; i < player_count; i++)
	{
		char *address = (char *)original_get_player_by_id(last_a1, i);
		if (!address)
		{
			break;
		}
		bool is_impostor = *((bool *)address + 0x28);
		char color = get_color(address) + '0';
		if (is_impostor)
		{
			output += color;
			output += ' ';
		}
	}
	output += "\r\n";
	write_pipe(output.c_str());
}

bool hook_canmove(int a1)
{
	return true;
}

void disable_canmove()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourAttach(&(LPVOID&)canmove_addr, &hook_canmove);
	DetourTransactionCommit();
}

void enable_canmove()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourDetach(&(LPVOID&)canmove_addr, &hook_canmove);
	DetourTransactionCommit();

}

typedef int(*setcooldown)(int instance, int time);
int hook_setcooldown(int instance, int time)
{
	setcooldown original_setcooldown = (setcooldown)setcooldown_addr;

	return original_setcooldown(instance, 0);
}

void disable_cooldown()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourAttach(&(LPVOID&)setcooldown_addr, &hook_setcooldown);
	DetourTransactionCommit();
}

void enable_cooldown()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourDetach(&(LPVOID&)setcooldown_addr, &hook_setcooldown);
	DetourTransactionCommit();
}

void check_pipe()
{
	char command;
	DWORD bytes_available = 0;
	DWORD buffer_size = 0;
	PeekNamedPipe(read_file_handle, &buffer_size, NULL, NULL,
		&bytes_available, NULL);
	if (!bytes_available)
	{
		return;
	}
	memset(buffer, 0, 200);
	read_string(buffer);
	std::string message(buffer);
	if (message.size() == 0)
	{
		return;
	}
	std::stringstream string_stream(message);
	string_stream >> command;
	switch (command)
	{
	case 'c':
		complete_tasks(string_stream);
		break;
	case 'a':
		disable_canmove();
		break;
	case 'b':
		enable_canmove();
		break;
	case 'k':
		disable_cooldown();
		break;
	case 'd':
		enable_cooldown();
		break;
	default:
		break;
	}
	
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

		buffer = new char[200];
		setup_pipe();
		getpinfo_addr = sig_scan("##558BEC803D2B068579007515FF35E0CC5979E8D907EFFF83C404C6052B068579018A550C53565780FAFF746B8B450833FF33C98B402485C074648BF08D5F10903B480C7D5285F674553B7E0C720D6A00E87B13FAFF8A550C83C4048B46088B0C"); // GameData.GetPlayerById
		getpcontrol_addr = sig_scan("##558BEC803D38068579007515FF3578BB5779E8899EEEFF83C404C6053806857901A1F86F8D79538B5D0856F680BB000000028B732C740F83787400750950E84D94E8FF83C4046A0056E83234350083C40884C0755DA1082B8D79F680BB000000"); // PlayerInfo.get_Object
		completetask_addr = sig_scan("##558BEC51803D81058579007515FF3598B85779E8885DF0FF83C404C6058105857901535657FF35A8178D79E8A05EF0FF8BF86A0057E856D1F6FF83C40C85FF0F84460100008B4D0C8B5D08894F08FF3598178D798B7374E8745EF0FFFF35F039"); // PlayerControl.CompleteTask
		late_update_addr = sig_scan("##558BEC83EC2C6A00FF7508E8105C3D0083C40885C074646A00508D45D450E8CDD740006A00FF7508F30F7E00660FD645F8F30F7EC0660FD645ECF30F1045F0F30F5E0550646A79F30F1145E8E8CF5B3D0083C41485C07423F30F7E45F88B4DE8"); // PlayerPhysics.LateUpdate
		get_pcount_addr = sig_scan("##558BEC803D27068579007515FF3590CD5979E8A9F4EEFF83C404C60527068579018B45088B402485C074058B400C5DC3E9FBF5EEFFCCCCCCCCCCCCCCCCCCCCCC558BEC83EC10803D40068579005356577515FF359CCD5979E863F4EEFF83C404"); // GameData.get_PlayerCount
		canmove_addr = sig_scan("##558BEC803D6B058579007515FF35D4BA5779E81903F0FF83C404C6056B058579018B450856807830000F8427020000A1BC2A8D798B405C8B30A1F86F8D79F680BB00000002740F83787400750950E8CDF8E9FF83C4046A0056E8B298360083C4"); // PlayerControl.get_CanMove
		setcooldown_addr = sig_scan("##558BEC803D6D058579007515FF3534BA5779E8C915F0FF83C404C6056D05857901F30F10450C568B7508F30F114644A1082B8D79F680BB00000002741483787400750E50E8870BEAFFA1082B8D7983C4048B405C8B400485C00F84F2000000F3"); // PlayerControl.SetKillTimer

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		DetourAttach(&(LPVOID&)getpinfo_addr, &hook_get_player_by_id);
		DetourAttach(&((LPVOID&)late_update_addr), &hook_late_update);
		DetourAttach(&((LPVOID&)get_pcount_addr), &hook_get_player_count);

		DetourTransactionCommit();

		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		DetourDetach(&(LPVOID&)getpinfo_addr, &hook_get_player_by_id);
		DetourDetach(&((LPVOID&)late_update_addr), &hook_late_update);
		DetourDetach(&((LPVOID&)get_pcount_addr), &hook_get_player_count);
		DetourTransactionCommit();
        break;
    }
    return TRUE;
}

