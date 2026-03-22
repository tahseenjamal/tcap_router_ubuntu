#ifndef M3UA_SERVER_H
#define M3UA_SERVER_H

void m3ua_server_start(int port);
void *m3ua_server_thread(void *arg);

#endif
