// GENERATED FILE, DO NOT MODIFY

extern int SystemNative_ConvertErrorPlatformToPal (int);
extern int SystemNative_ConvertErrorPalToPlatform (int);
extern int SystemNative_StrErrorR (int,int,int);
extern void SystemNative_GetNonCryptographicallySecureRandomBytes (int,int);
extern int SystemNative_OpenDir (int);
extern int SystemNative_GetReadDirRBufferSize (void);
extern int SystemNative_ReadDirR (int,int,int,int);
extern int SystemNative_CloseDir (int);
extern int SystemNative_ReadLink (int,int,int);
extern int SystemNative_FStat2 (int,int);
extern int SystemNative_Stat2 (int,int);
extern int SystemNative_LStat2 (int,int);
extern int SystemNative_Symlink (int,int);
extern int SystemNative_ChMod (int,int);
extern int SystemNative_CopyFile (int,int);
extern int SystemNative_GetEGid (void);
extern int SystemNative_GetEUid (void);
extern int SystemNative_LChflags (int,int);
extern int SystemNative_LChflagsCanSetHiddenFlag (void);
extern int SystemNative_Link (int,int);
extern int SystemNative_MkDir (int,int);
extern int SystemNative_Rename (int,int);
extern int SystemNative_RmDir (int);
extern int SystemNative_Stat2 (int,int);
extern int SystemNative_LStat2 (int,int);
extern int SystemNative_UTime (int,int);
extern int SystemNative_UTimes (int,int);
extern int SystemNative_Unlink (int);

static MonoDlMapping system_native_mappings[] = {
    {"SystemNative_ConvertErrorPlatformToPal", SystemNative_ConvertErrorPlatformToPal},
    {"SystemNative_ConvertErrorPalToPlatform", SystemNative_ConvertErrorPalToPlatform},
    {"SystemNative_StrErrorR", SystemNative_StrErrorR},
    {"SystemNative_GetNonCryptographicallySecureRandomBytes", SystemNative_GetNonCryptographicallySecureRandomBytes},
    {"SystemNative_OpenDir", SystemNative_OpenDir},
    {"SystemNative_GetReadDirRBufferSize", SystemNative_GetReadDirRBufferSize},
    {"SystemNative_ReadDirR", SystemNative_ReadDirR},
    {"SystemNative_CloseDir", SystemNative_CloseDir},
    {"SystemNative_ReadLink", SystemNative_ReadLink},
    {"SystemNative_FStat2", SystemNative_FStat2},
    {"SystemNative_Stat2", SystemNative_Stat2},
    {"SystemNative_LStat2", SystemNative_LStat2},
    {"SystemNative_Symlink", SystemNative_Symlink},
    {"SystemNative_ChMod", SystemNative_ChMod},
    {"SystemNative_CopyFile", SystemNative_CopyFile},
    {"SystemNative_GetEGid", SystemNative_GetEGid},
    {"SystemNative_GetEUid", SystemNative_GetEUid},
    {"SystemNative_LChflags", SystemNative_LChflags},
    {"SystemNative_LChflagsCanSetHiddenFlag", SystemNative_LChflagsCanSetHiddenFlag},
    {"SystemNative_Link", SystemNative_Link},
    {"SystemNative_MkDir", SystemNative_MkDir},
    {"SystemNative_Rename", SystemNative_Rename},
    {"SystemNative_RmDir", SystemNative_RmDir},
    {"SystemNative_Stat2", SystemNative_Stat2},
    {"SystemNative_LStat2", SystemNative_LStat2},
    {"SystemNative_UTime", SystemNative_UTime},
    {"SystemNative_UTimes", SystemNative_UTimes},
    {"SystemNative_Unlink", SystemNative_Unlink},
    {NULL, NULL}
};

