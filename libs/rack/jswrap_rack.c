#include "jswrap_rack.h"
#include "jswrap_array.h"
#include "jswrap_arraybuffer.h"
#include "jswrap_interactive.h"
#include "jswrapper.h"


#define MTU 53
#define MSG_SIZE (MTU - 3)
#define PART (MSG_SIZE - 1)
#define WD_MS (2 * 1000)


/*
 * INIT(n)
 * `n` .. max number of locks
 */
#define INIT(n) JsVar* _lock_array[n]; int _lock_index = 0
#define _LOCK(v) _lock_array[_lock_index++] = v
#define _UNLOCK(n) for (int i = 0; i < n; i++) jsvUnLock(_lock_array[--_lock_index])
/*
 * V{n}(...)
 * - if pointer is 0, goto `out_of_memory`
 * - unlock last `n` objects
 * - lock object
 */
#define _V(a, n) ({ JsVar* _js_var = a; if (!_js_var) goto out_of_memory; _UNLOCK(n); _LOCK(_js_var); _js_var; })
#define V(a) _V(a, 0)   // +1 lock
#define V1(a) _V(a, 1)  // no change
#define V2(a) _V(a, 2)  // -1 lock
#define V3(a) _V(a, 3)  // -2 locks
/*
 * I{n}(...)
 * - unlock last `n` objects
 */
#define _I(a, n) ({ JsVarInt _var_int = a; _UNLOCK(n); _var_int; })
#define I(a) _I(a, 0)   // no change
#define I1(a) _I(a, 1)  // -1 lock
#define I2(a) _I(a, 2)  // -2 locks
#define I3(a) _I(a, 3)  // -3 locks
/*
 * G{t}{n}(...)
 * "GUARD"
 * - if `!...`, goto `out_of_memory`
 * - unlock last `n` objects
 */
#define _G(t, a, n) ({ t _object = a; if (!_object) goto out_of_memory; _UNLOCK(n); _object; })
#define GV(a) _G(JsVar*, a, 0)   // no change
#define GV1(a) _G(JsVar*, a, 1)  // -1 lock
#define GV2(a) _G(JsVar*, a, 2)  // -2 locks
#define GV3(a) _G(JsVar*, a, 3)  // -3 locks
#define GI(a) _G(JsVarInt, a, 0)   // no change
#define GI1(a) _G(JsVarInt, a, 1)  // -1 lock
#define GI2(a) _G(JsVarInt, a, 2)  // -2 locks
#define GI3(a) _G(JsVarInt, a, 3)  // -3 locks
/*
 * RETURN
 * - unlock all objects
 */
#define RETURN while (_lock_index != 0) jsvUnLock(_lock_array[--_lock_index])


/*JSON{
  "type" : "class",
  "class" : "Rack"
}*/

/*JSON{
  "type" : "staticmethod",
  "class" : "Rack",
  "name" : "Session",
  "generate" : "jswrap_rack_Session",
  "params" : [
    ["id", "int", "TODO"],
    ["rfun", "JsVar", "TODO"],
    ["wfun", "JsVar", "TODO"]
  ],
  "return": ["JsVar", "TODO"]
}*/
JsVar* jswrap_rack_Session(int id, JsVar* read_function, JsVar* write_function) {  // var Session = function(id, rfun, wfun) {
  INIT(2);

  JsVar* object = V(jsvNewObject());
  GV1(jsvObjectSetChild(object, "_fs", V(jsvNewFlatStringOfLength(sizeof(_Session)))));

  _Session* _session = _get_session(object);

  _session->id = (uint8_t) id;  // this.id = id;
  GV(jsvObjectSetChild(object, "rfun", read_function));  // this.rfun = rfun;
  GV(jsvObjectSetChild(object, "wfun", write_function));  // this.wfun = wfun;

  GV1(jsvObjectSetChild(object, "wque", V(jsvNewEmptyArray())));  // this.wque = [];
  _session->wcnt = 0;  // this.wcnt = 0;
  _session->widx = 0;  // this.widx = 0;
  _session->woff = 0;  // this.woff = 0;
  _session->wid = 0;  // this.wid = 0;
  _session->aid = 0;  // this.aid = 0;
  _session->aoff = 0;  // this.aoff = 0;
  _session->rid = 0;  // this.rid = 0;
  _session->wdd = 0;

  return object;

out_of_memory:
  RETURN;
  return 0;
}


/*JSON{
  "type" : "staticmethod",
  "class" : "Rack",
  "name" : "write",
  "generate" : "jswrap_rack_Session_write",
  "params" : [
    ["session", "JsVar", "TODO"],
    ["array", "JsVar", "TODO"],
    ["callback", "JsVar", "TODO"]
  ]
}*/
void jswrap_rack_Session_write(JsVar* session, JsVar* array, JsVar* callback_function) {  // Session.prototype.write = function(arr, cbf) {
  if (!session) return;

  INIT(2);

  // this.wque.push(arr, cbf);
  JsVar* q = V(jsvObjectGetChild(session, "wque", 0));
  GI(jsvArrayPush(q, array));
  I1(jsvArrayPush(q, callback_function));

  _Session* _session = _get_session(session);

  while (_session->wcnt < 4 && _wrt(session)) _session->wcnt++;  // while (this.wcnt < 4 && this._wrt()) this.wcnt++;
  if (!_session->wdd) {  // if (!this.wdt) {
    _session->wdd = jshGetSystemTime();  // this.wdd = new Date();

    // this.wdt = setTimeout(this._wd.bind(this), WD_MS);
    JsVar* function = V(jsvNewNativeFunction((void (*)(void))_wd, JSWAT_THIS_ARG));
    GV(jsvObjectSetChild(function, JSPARSE_FUNCTION_THIS_NAME, session));
    GV2(V(jswrap_interface_setTimeout(function, WD_MS, 0)));
  }

  return;

out_of_memory:
  RETURN;
}


/*JSON{
  "type" : "staticmethod",
  "class" : "Rack",
  "name" : "_read",
  "generate" : "jswrap_rack_Session__read",
  "params" : [
    ["session", "JsVar", "TODO"],
    ["array", "JsVar", "TODO"]
  ]
}*/
void jswrap_rack_Session__read(JsVar* session, JsVar* array) {  // Session.prototype._read = function(arr) {
  if (!session) return;

  INIT(3);

  _Session* _session = _get_session(session);

  if (jsvGetArrayBufferLength(array) == 1) {  // if (arr.length == 1) {  // ack
    if ((I1(jsvGetInteger(V(jsvArrayBufferGet(array, 0)))) & 0xf) != (_session->aid & 0xf)) return;  // if ((arr[0] & 0xf) != (this.aid & 0xf)) return;
    _session->aid++;  // this.aid++;
    _session->aoff += PART;  // var o = this.aoff + PART;
    JsVar* q = V(jsvObjectGetChild(session, "wque", 0));  // var q = this.wque;
    if (I1((JsVarInt) jsvGetArrayBufferLength(V(jsvGetArrayItem(q, 0)))) <= _session->aoff) {  // if (q[0].length <= o) {
      jsvUnLock(jspeFunctionCall(V1(jsvGetArrayItem(V2(jswrap_array_splice(q, 0, V(jsvNewFromInteger(2)), V(jsvNewEmptyArray()))), 1)), 0, 0, false, 0, 0)); _UNLOCK(1);  // q.splice(0, 2)[1]();
      _session->widx -= 2;  // this.widx -= 2;
      _session->aoff = 0;  // o = 0;
    }
    _UNLOCK(1);  // q
    // this.aoff = o;
    if (!_wrt(session)) _session->wcnt--;  // if (!this._wrt()) this.wcnt--;
    _session->wdd = jshGetSystemTime();  // this.wdd = new Date();

  } else {
    JsVar* args[1];

    // this.wfun(arr.subarray(0, 1));  // ack
    args[0] = V1(jswrap_arraybufferview_subarray(array, 0, V(jsvNewFromInteger(1))));
    jsvUnLock(jspeFunctionCall(V(jsvObjectGetChild(session, "wfun", 0)), 0, 0, false, 1, args)); _UNLOCK(2);

    if ((I1(jsvGetInteger(V(jsvArrayBufferGet(array, 0)))) & 0xf) != (_session->rid & 0xf)) return;  // if ((arr[0] & 0xf) != (this.rid & 0xf)) return;

    // this.rfun(arr.subarray(1));
    args[0] = V(jswrap_arraybufferview_subarray(array, 1, 0));
    jsvUnLock(jspeFunctionCall(V(jsvObjectGetChild(session, "rfun", 0)), 0, 0, false, 1, args)); _UNLOCK(2);

    _session->rid++;  // this.rid++;
  }

  return;

out_of_memory:
  RETURN;
}


void _wd(JsVar* session) {  // Session.prototype._wd = function() {
  INIT(2);

  _Session* _session = _get_session(session);

  JsVarFloat ms = jshGetMillisecondsFromTime(jshGetSystemTime() - _session->wdd);  // var ms = (new Date()) - this.wdd;
  if (ms < WD_MS) {  // if (ms < WD_MS) {
    // this.wdt = setTimeout(this._wd.bind(this), WD_MS - ms);
    JsVar* function = V(jsvNewNativeFunction((void (*)(void))_wd, JSWAT_THIS_ARG));
    GV(jsvObjectSetChild(function, JSPARSE_FUNCTION_THIS_NAME, session));
    GV2(V(jswrap_interface_setTimeout(function, WD_MS - ms, 0)));

  } else if (I1(jsvGetArrayLength(V(jsvObjectGetChild(session, "wque", 0)))) != 0) {  // } else if (this.wque.length) {
    _session->wcnt = 0;  // this.wcnt = 0;
    _session->widx = 0;  // this.widx = 0;
    _session->woff = _session->aoff;  // this.woff = this.aoff;
    _session->wid = _session->aid;  // this.wid = this.aid;
    while (_session->wcnt < 4 && _wrt(session)) _session->wcnt++;  // while (this.wcnt < 4 && this._wrt()) this.wcnt++;

    // this.wdt = setTimeout(this._wd.bind(this), WD_MS);
    JsVar* function = V(jsvNewNativeFunction((void (*)(void))_wd, JSWAT_THIS_ARG));
    GV(jsvObjectSetChild(function, JSPARSE_FUNCTION_THIS_NAME, session));
    GV2(V(jswrap_interface_setTimeout(function, WD_MS, 0)));

  } else {
    _session->wdd = 0;  // this.wdt = undefined;
  }

  return;

out_of_memory:
  RETURN;
}


bool _wrt(JsVar* session) {  // Session.prototype._wrt = function() {
  INIT(3);

  JsVar* q = V(jsvObjectGetChild(session, "wque", 0));  // var q = this.wque;
  // var i = this.widx;

  _Session* _session = _get_session(session);

  if (jsvGetArrayLength(q) <= _session->widx) { _UNLOCK(1); return false; }  // if (q.length <= i) return false;
  // var o = this.woff;
  JsVar* array = V1(jsvGetArrayItem(q, _session->widx));  // var arr = q[i];
  JsVar* message = V(jsvNewTypedArray(ARRAYBUFFERVIEW_UINT8, 1 + _min(_session->woff + PART, (int) jsvGetArrayBufferLength(array)) - _session->woff));  // var msg = new Uint8Array(1 + Math.min(o + PART, arr.length) - o);
  jsvArrayBufferSet(message, 0, V(jsvNewFromInteger((_session->id << 4) | (_session->wid++ & 0xf)))); _UNLOCK(1);  // msg[0] = (this.id << 4) | (this.wid++ & 0xf);
  jswrap_arraybufferview_set(message, V(jswrap_arraybufferview_subarray(array, _session->woff, 0)), 1); _UNLOCK(1);  // msg.set(arr.subarray(o, o + msg.length - 1), 1);

  // this.wfun(msg);
  JsVar* args[1] = {message};
  jsvUnLock(jspeFunctionCall(V(jsvObjectGetChild(session, "wfun", 0)), 0, 0, false, 1, args)); _UNLOCK(2);

  _session->woff += PART;  // o += PART;
  if (jsvGetArrayBufferLength(array) <= _session->woff) {  // if (arr.length <= o) {
    _session->widx += 2;  // this.widx += 2;
    _session->woff = 0;  // o = 0;
  }
  _UNLOCK(1);  // array
  // this.woff = o;
  return true;  // return true;

out_of_memory:
  RETURN;
  return false;
}


/*JSON{
  "type" : "staticmethod",
  "class" : "Rack",
  "name" : "read",
  "generate" : "jswrap_rack_read",
  "params" : [
    ["array", "JsVar", "TODO"],
    ["sessions", "JsVar", "TODO"]
  ]
}*/
void jswrap_rack_read(JsVar* array, JsVar* sessions) {
  INIT(1);

  JsVar* session = jsvGetArrayItem(sessions, I1(jsvGetInteger(V(jsvArrayBufferGet(array, 0)))) >> 4);  // var session = sessions[array[0] >> 4];
  if (!session) return;  // if (!session) return;
  jswrap_rack_Session__read(session, array);  // Rack._read(session, array);
  jsvUnLock(session);

  return;

out_of_memory:
  RETURN;
}


_Session* _get_session(JsVar* session) {
  JsVar* flat_string = jsvObjectGetChild(session, "_fs", 0);
  size_t _;
  _Session* _session = (_Session*) jsvGetDataPointer(flat_string, &_);
  jsvUnLock(flat_string);
  return _session;
}


int _min(int a, int b) {
  return a < b ? a : b;
}
