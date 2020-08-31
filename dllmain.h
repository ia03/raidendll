__declspec(dllimport) DWORD SigScan(const char* szPattern, int offset = 0);
__declspec(dllimport) void InitializeSigScan(DWORD ProcessID, const char* Module);
__declspec(dllimport) void FinalizeSigScan();
#pragma comment(lib,"SigScan.lib")

DWORD getpinfo_addr = 0;
DWORD revive_addr = 0;
DWORD late_update_addr = 0;
DWORD subgetpinfo_addr = 0;
DWORD get_pcount_addr = 0;
DWORD player_array = 0;
DWORD prev_player_array = 0;
DWORD shadow_addr = 0;
DWORD canmove_addr = 0;
DWORD completetask_addr = 0;
DWORD getpcontrol_addr = 0;
DWORD setcooldown_addr = 0;
int player_count = 0;
int last_a1 = 0;
char* buffer;

HANDLE write_file_handle;
HANDLE read_file_handle;


typedef int(*get_player_info)(int a1, char id);
typedef int(*completetask)(char *player_control, int index);
typedef char *(*getpcontrol)(char *player_info);

void check_pipe();
void send_impostors();
int frame_count = 0;