 @ stdcall CreateEnvironmentBlock(ptr ptr long)
 @ stdcall DestroyEnvironmentBlock(ptr)
 @ stdcall ExpandEnvironmentStringsForUserA(ptr str ptr long)
 @ stdcall ExpandEnvironmentStringsForUserW(ptr wstr ptr long)
 @ stdcall GetProfilesDirectoryA(ptr ptr)
 @ stdcall GetProfilesDirectoryW(ptr ptr)
 @ stub GetProfileType
 @ stdcall GetUserProfileDirectoryA(ptr ptr ptr)
 @ stdcall GetUserProfileDirectoryW(ptr ptr ptr)
 @ stdcall LoadUserProfileA(ptr ptr)
 @ stdcall LoadUserProfileW(ptr ptr)
 @ stdcall RegisterGPNotification(long long)
 @ stdcall UnloadUserProfile(ptr ptr)
 @ stdcall UnregisterGPNotification(long)
100 stdcall -noname InitializeProfiles()
101 stdcall -noname CreateGroupA(str long)
102 stdcall -noname CreateGroupW(wstr long)
103 stdcall -noname DeleteGroupA(str long)
104 stdcall -noname DeleteGroupW(wstr long)
105 stdcall -noname AddItemA(str long str str str long str long long)
106 stdcall -noname AddItemW(wstr long wstr wstr wstr long wstr long long)
107 stdcall -noname DeleteItemA(str long str long)
108 stdcall -noname DeleteItemW(wstr long wstr long)
109 stdcall -noname CreateUserProfileA(ptr str)
110 stdcall -noname CreateUserProfileW(ptr wstr)
111 stdcall -noname CopyProfileDirectoryA(str str long)
112 stdcall -noname CopyProfileDirectoryW(wstr wstr long)
113 stdcall -noname AddDesktopItemA(long str str str long str long long)
114 stdcall -noname AddDesktopItemW(long wstr wstr wstr long wstr long long)
115 stdcall -noname DeleteDesktopItemA(long str)
116 stdcall -noname DeleteDesktopItemW(long wstr)
 @ stdcall EnterCriticalPolicySection(long)
 @ stdcall GetAllUsersProfileDirectoryA(str ptr)
 @ stdcall GetAllUsersProfileDirectoryW(wstr ptr)
 @ stdcall GetDefaultUserProfileDirectoryA(str ptr)
 @ stdcall GetDefaultUserProfileDirectoryW(wstr ptr)
 @ stdcall LeaveCriticalPolicySection(long)
 @ stdcall RefreshPolicy(long)
 @ stdcall RefreshPolicyEx(long long)
 @ stdcall WaitForUserPolicyForegroundProcessing()
 @ stdcall WaitForMachinePolicyForegroundProcessing()
 @ stdcall DeleteProfileW(wstr wstr wstr)
 @ stdcall DeleteProfileA(str str str)
