#ifndef ATENDER_IO_H_
#define ATENDER_IO_H_

#include "base_sch.h"

void* atender_io(t_io* io);
void atender_sys(t_io* io, t_paquete* paquete_envio, char* sys_name);
void atender_io_sleep(t_io* io);
void atender_io_stdin(t_io* io);
void atender_io_stdout(t_io* io);
#endif /* ATENDER_IO_H_ */