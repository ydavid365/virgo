/*
 *  Copyright 2011 Rackspace
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "virgo.h"
#include "virgo__types.h"
#include "virgo__lua.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luaconf.h"

#include "unzip.h"

#include <stdlib.h>
#include <string.h>

static int
virgo__lua_loader_checkload(lua_State *L, int stat, const char *filename) {

  if (stat) {  /* module loaded successfully? */
    lua_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else {
    return luaL_error(L, "error loading virgo module " LUA_QS
                         " from file " LUA_QS ":\n\t%s",
                          lua_tostring(L, 1), filename, lua_tostring(L, -1));
  }
}

static int
virgo__lua_loader_zip2buf(virgo_t* v, const char *name, char **p_buf, size_t *p_len)
{
  struct unz_file_info_s finfo = {0};
  unzFile zip = NULL;
  char *buf;
  size_t len;
  int rv;
  int rc = 0;
  /* TODO: would be nice to have better error handling / messages from this method */

  *p_buf = NULL;
  *p_len = 0;

  zip = unzOpen(v->lua_load_path);

  if (zip == NULL) {
    rc = -1;
    goto cleanup;
  }

  /* 1 means case sensitive file comparison */
  rv = unzLocateFile(zip, name, 1);
  if (rv != UNZ_OK) {
    rc = -2;
    goto cleanup;
  }

  rv = unzGetCurrentFileInfo(zip,
                             &finfo,
                             NULL, 0,
                             NULL, 0,
                             NULL, 0);
  if (rv != UNZ_OK) {
    rc = -3;
    goto cleanup;
  }

  buf = malloc(finfo.uncompressed_size);
  len = finfo.uncompressed_size;

  rv = unzReadCurrentFile(zip, buf, len);
  if (rv != len) {
    free(buf);
    rc = -4;
    goto cleanup;
  }

  *p_buf = buf;
  *p_len = len;

cleanup:
  if (zip) {
    unzCloseCurrentFile(zip);
    unzClose(zip);
  }

  return rc;
}

static int
virgo__lua_loader_loadit(lua_State *L) {
  char *buf = NULL;
  size_t len = 0;
  int rv;
  virgo_t* v = virgo__lua_context(L);
  const char *name = luaL_checkstring(L, 1);
  size_t nlen = strlen(name) + strlen(".lua") + 1;
  char *nstr = malloc(nlen);
  snprintf(nstr, nlen, "%s.lua", name);

  rv = virgo__lua_loader_zip2buf(v, nstr, &buf, &len);
  if (rv != 0) {
    free(nstr);
    return luaL_error(L, "error finding virgo module in zip: (%d) %s", rv, nstr);
  }

  rv = luaL_loadbuffer(L, buf, len, nstr);

  free(buf);
  free(nstr);

  return virgo__lua_loader_checkload(L, rv == LUA_OK, name);
}

void
virgo__lua_loader_init(lua_State *L)
{
  int top;

  top = lua_gettop(L);
  lua_getglobal(L, "package");

  if (lua_type(L, -1) != LUA_TTABLE) {
    abort();
  }

  lua_pushliteral(L, "searchers");
  lua_gettable(L, -2);

  if(lua_type(L, -1) != LUA_TTABLE) {
    abort();
  }

  lua_pushnumber(L, lua_rawlen(L, -1) + 1);
  lua_pushcfunction(L, virgo__lua_loader_loadit);
  lua_settable(L, -3);
}

