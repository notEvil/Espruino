#include "jsvar.h"


typedef struct {
  uint8_t id;
  uint8_t wcnt;
  uint8_t widx;
  uint16_t woff;
  uint8_t wid;
  uint8_t aid;
  uint16_t aoff;
  uint8_t rid;
  JsSysTime wdd;
} PACKED_FLAGS _Session;


JsVar* jswrap_rack_Session(int id, JsVar* read_function, JsVar* write_function);
void jswrap_rack_Session_write(JsVar* session, JsVar* array, JsVar* callback_function);
void jswrap_rack_Session__read(JsVar* session, JsVar* array);
void _wd(JsVar* session);
bool _wrt(JsVar* session);
void jswrap_rack_read(JsVar* array, JsVar* sessions);

_Session* _get_session(JsVar* session);
int _min(int a, int b);
