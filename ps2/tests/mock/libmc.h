#ifndef TEST_PS2_MOCK_LIBMC_H
#define TEST_PS2_MOCK_LIBMC_H
#define MC_TYPE_MC 0
int mcInit(int type);
int mcGetInfo(int port,int slot,int *type,int *free_space,int *format);
int mcSync(int mode,int *cmd,int *result);
int mcOpen(int port,int slot,const char *name,int mode);
int mcClose(int fd);
int mcSeek(int fd,int offset,int origin);
int mcRead(int fd,void *data,int size);
int mcWrite(int fd,const void *data,int size);
int mcFlush(int fd);
int mcMkDir(int port,int slot,const char *name);
int mcDelete(int port,int slot,const char *name);
#endif
