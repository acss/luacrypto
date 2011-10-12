/*
** $Id: lcrypto.c,v 1.2 2006/08/25 03:24:17 nezroy Exp $
** See Copyright Notice in license.html
*/

#include <string.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/pem.h>

#ifdef __cplusplus
#include "lua.hpp"
#else
#include "lua.h"
#endif
#include "lauxlib.h"
#if ! defined (LUA_VERSION_NUM) || LUA_VERSION_NUM < 501
#include "compat-5.1.h"
#endif

#include "lcrypto.h"

#ifdef __cplusplus
extern "C"
#endif
LUACRYPTO_API int luaopen_crypto(lua_State *L);

static int crypto_error(lua_State *L)
{
  char buf[120];
  unsigned long e = ERR_get_error();
  ERR_load_crypto_strings();
  lua_pushnil(L);
  lua_pushstring(L, ERR_error_string(e, buf));
  return 2;
}

/*************** DIGEST API ***************/

static EVP_MD_CTX *digest_pnew(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)lua_newuserdata(L, sizeof(EVP_MD_CTX));
  luaL_getmetatable(L, LUACRYPTO_DIGESTNAME);
  lua_setmetatable(L, -2);
  return c;
}

static int digest_fnew(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  const EVP_MD *digest = EVP_get_digestbyname(s);
  
  if (digest == NULL) {
    luaL_argerror(L, 1, "invalid digest/cipher type");
    return 0;
  } else {
    EVP_MD_CTX *c = digest_pnew(L);
    EVP_MD_CTX_init(c);
    EVP_DigestInit_ex(c, digest, NULL);
    return 1;
  }
}

static int digest_clone(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DIGESTNAME);
  EVP_MD_CTX *d = digest_pnew(L);
  EVP_MD_CTX_init(d);
  EVP_MD_CTX_copy_ex(d, c);
  return 1;
}

static int digest_reset(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DIGESTNAME);
  const EVP_MD *t = EVP_MD_CTX_md(c);
  EVP_MD_CTX_cleanup(c);
  EVP_MD_CTX_init(c);
  EVP_DigestInit_ex(c, t, NULL);
  return 0;
}

static int digest_update(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DIGESTNAME);
  const char *s = luaL_checkstring(L, 2);
  
  EVP_DigestUpdate(c, s, lua_strlen(L, 2));
  
  lua_settop(L, 1);
  return 1;
}

static int digest_final(lua_State *L) 
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DIGESTNAME);
  EVP_MD_CTX *d = NULL;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int written = 0;
  unsigned int i;
  char *hex;
  
  if (lua_isstring(L, 2))
  {  
    const char *s = luaL_checkstring(L, 2);
    EVP_DigestUpdate(c, s, lua_strlen(L, 2));
  }
  
  d = EVP_MD_CTX_create();
  EVP_MD_CTX_copy_ex(d, c);
  EVP_DigestFinal_ex(d, digest, &written);
  EVP_MD_CTX_destroy(d);
  
  if (lua_toboolean(L, 3))
    lua_pushlstring(L, (char *)digest, written);
  else
  {
    hex = (char*)calloc(sizeof(char), written*2 + 1);
    for (i = 0; i < written; i++)
      sprintf(hex + 2*i, "%02x", digest[i]);
    lua_pushlstring(L, hex, written*2);
    free(hex);
  }
  
  return 1;
}

static int digest_tostring(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DIGESTNAME);
  char s[64];
  sprintf(s, "%s %p", LUACRYPTO_DIGESTNAME, (void *)c);
  lua_pushstring(L, s);
  return 1;
}

static int digest_gc(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DIGESTNAME);
  EVP_MD_CTX_cleanup(c);
  return 1;
}

static int digest_fdigest(lua_State *L)
{
  EVP_MD_CTX *c = NULL;
  const char *type_name = luaL_checkstring(L, 2);
  const char *s = luaL_checkstring(L, 3);
  const EVP_MD *type = EVP_get_digestbyname(type_name);
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int written = 0;
  unsigned int i;
  char *hex;
  
  if (type == NULL) {
    luaL_argerror(L, 1, "invalid digest type");
    return 0;
  }
  
  c = EVP_MD_CTX_create();
  EVP_DigestInit_ex(c, type, NULL);
  EVP_DigestUpdate(c, s, lua_strlen(L, 3));
  EVP_DigestFinal_ex(c, digest, &written);
  
  if (lua_toboolean(L, 4))
    lua_pushlstring(L, (char *)digest, written);
  else
  {
    hex = (char*)calloc(sizeof(char), written*2 + 1);
    for (i = 0; i < written; i++)
      sprintf(hex + 2*i, "%02x", digest[i]);
    lua_pushlstring(L, hex, written*2);
    free(hex);
  }
  
  return 1;
}

/*************** ENCRYPT API ***************/

static EVP_CIPHER_CTX *encrypt_pnew(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)lua_newuserdata(L, sizeof(EVP_CIPHER_CTX));
  luaL_getmetatable(L, LUACRYPTO_ENCRYPTNAME);
  lua_setmetatable(L, -2);
  return c;
}

static int encrypt_fnew(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  const EVP_CIPHER *cipher = EVP_get_cipherbyname(s);
  if (cipher == NULL) {
    luaL_argerror(L, 1, "invalid encrypt cipher");
    return 0;
  } else {
    size_t key_len = 0;
    const char *key = luaL_checklstring(L, 2, &key_len);
    unsigned char evp_key[EVP_MAX_KEY_LENGTH] = {0};
  
    size_t iv_len = 0;
    const char *iv = lua_tolstring(L, 3, &iv_len); /* can be NULL */
    unsigned char evp_iv[EVP_MAX_IV_LENGTH] = {0};
  
    memcpy(evp_key, key, key_len);
    if (iv) {
      memcpy(evp_iv, iv, iv_len);      
    }
    
    EVP_CIPHER_CTX *c = encrypt_pnew(L);
    EVP_CIPHER_CTX_init(c);
    EVP_EncryptInit_ex(c, cipher, NULL, evp_key, iv ? evp_iv : NULL);
    return 1;
  }
}

static int encrypt_update(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_ENCRYPTNAME);
  size_t input_len = 0;
  const unsigned char *input = (unsigned char *) luaL_checklstring(L, 2, &input_len);
  int output_len = 0;
  unsigned char *buffer = NULL;

  buffer = (unsigned char*)malloc(input_len + EVP_CIPHER_CTX_block_size(c));
  EVP_EncryptUpdate(c, buffer, &output_len, input, input_len);
  lua_pushlstring(L, (char*) buffer, output_len);
  free(buffer);

  return 1;
}

static int encrypt_final(lua_State *L) 
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_ENCRYPTNAME);
  int output_len = 0;
  unsigned char buffer[EVP_MAX_BLOCK_LENGTH];
  
  EVP_EncryptFinal(c, buffer, &output_len);
  lua_pushlstring(L, (char*) buffer, output_len);
  return 1;
}

static int encrypt_tostring(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_ENCRYPTNAME);
  char s[64];
  sprintf(s, "%s %p", LUACRYPTO_ENCRYPTNAME, (void *)c);
  lua_pushstring(L, s);
  return 1;
}

static int encrypt_gc(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_ENCRYPTNAME);
  EVP_CIPHER_CTX_cleanup(c);
  return 1;
}

static int encrypt_fencrypt(lua_State *L)
{
  /* parameter 1 is the 'crypto.encrypt' table */
  const char *type_name = luaL_checkstring(L, 2);
  const EVP_CIPHER *type = EVP_get_cipherbyname(type_name);

  if (type == NULL) {
    luaL_argerror(L, 1, "invalid encrypt cipher");
    return 0;
  } else {
    EVP_CIPHER_CTX c;
  
    size_t input_len = 0;
    const unsigned char *input = (unsigned char *) luaL_checklstring(L, 3, &input_len);
  
    size_t key_len = 0;
    const char *key = luaL_checklstring(L, 4, &key_len);
    unsigned char evp_key[EVP_MAX_KEY_LENGTH] = {0};
  
    size_t iv_len = 0;
    const char *iv = lua_tolstring(L, 5, &iv_len); /* can be NULL */
    unsigned char evp_iv[EVP_MAX_IV_LENGTH] = {0};
  
    memcpy(evp_key, key, key_len);
    if (iv) {
      memcpy(evp_iv, iv, iv_len);      
    }
  
    int output_len = 0;
    int len = 0;
    unsigned char *buffer = NULL;
    
    EVP_CIPHER_CTX_init(&c);
    EVP_EncryptInit_ex(&c, type, NULL, evp_key, iv ? evp_iv : NULL);
    buffer = (unsigned char*)malloc(input_len + EVP_CIPHER_CTX_block_size(&c));
    EVP_EncryptUpdate(&c, buffer, &len, input, input_len);
    output_len += len;
    EVP_EncryptFinal(&c, &buffer[len], &len);
    output_len += len;
    
    lua_pushlstring(L, (char*) buffer, output_len);
    free(buffer);
    return 1;
  }
}

/*************** DECRYPT API ***************/

static EVP_CIPHER_CTX *decrypt_pnew(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)lua_newuserdata(L, sizeof(EVP_CIPHER_CTX));
  luaL_getmetatable(L, LUACRYPTO_DECRYPTNAME);
  lua_setmetatable(L, -2);
  return c;
}

static int decrypt_fnew(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  const EVP_CIPHER *cipher = EVP_get_cipherbyname(s);
  if (cipher == NULL) {
    luaL_argerror(L, 1, "invalid digest/cipher type");
    return 0;
  } else {
    size_t key_len = 0;
    const char *key = luaL_checklstring(L, 2, &key_len);
    unsigned char evp_key[EVP_MAX_KEY_LENGTH] = {0};
  
    size_t iv_len = 0;
    const char *iv = lua_tolstring(L, 3, &iv_len); /* can be NULL */
    unsigned char evp_iv[EVP_MAX_IV_LENGTH] = {0};
  
    memcpy(evp_key, key, key_len);
    if (iv) {
      memcpy(evp_iv, iv, iv_len);      
    }
    
    EVP_CIPHER_CTX *c = decrypt_pnew(L);
    EVP_CIPHER_CTX_init(c);
    EVP_DecryptInit_ex(c, cipher, NULL, evp_key, iv ? evp_iv : NULL);
    return 1;
  }
}

static int decrypt_update(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DECRYPTNAME);
  size_t input_len = 0;
  const unsigned char *input = (unsigned char *) luaL_checklstring(L, 2, &input_len);
  int output_len = 0;
  unsigned char *buffer = NULL;

  buffer = (unsigned char*)malloc(input_len + EVP_CIPHER_CTX_block_size(c));
  EVP_DecryptUpdate(c, buffer, &output_len, input, input_len);
  lua_pushlstring(L, (char*) buffer, output_len);
  free(buffer);

  return 1;
}

static int decrypt_final(lua_State *L) 
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DECRYPTNAME);
  int output_len = 0;
  unsigned char buffer[EVP_MAX_BLOCK_LENGTH];
  
  EVP_DecryptFinal(c, buffer, &output_len);
  lua_pushlstring(L, (char*) buffer, output_len);
  return 1;
}

static int decrypt_tostring(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DECRYPTNAME);
  char s[64];
  sprintf(s, "%s %p", LUACRYPTO_DECRYPTNAME, (void *)c);
  lua_pushstring(L, s);
  return 1;
}

static int decrypt_gc(lua_State *L)
{
  EVP_CIPHER_CTX *c = (EVP_CIPHER_CTX*)luaL_checkudata(L, 1, LUACRYPTO_DECRYPTNAME);
  EVP_CIPHER_CTX_cleanup(c);
  return 1;
}

static int decrypt_fdecrypt(lua_State *L)
{
  /* parameter 1 is the 'crypto.decrypt' table */
  const char *type_name = luaL_checkstring(L, 2);
  const EVP_CIPHER *type = EVP_get_cipherbyname(type_name);

  if (type == NULL) {
    luaL_argerror(L, 1, "invalid decrypt cipher");
    return 0;
  } else {
    EVP_CIPHER_CTX c;
  
    size_t input_len = 0;
    const unsigned char *input = (unsigned char *) luaL_checklstring(L, 3, &input_len);
  
    size_t key_len = 0;
    const char *key = luaL_checklstring(L, 4, &key_len);
    unsigned char evp_key[EVP_MAX_KEY_LENGTH] = {0};
  
    size_t iv_len = 0;
    const char *iv = lua_tolstring(L, 5, &iv_len); /* can be NULL */
    unsigned char evp_iv[EVP_MAX_IV_LENGTH] = {0};
  
    memcpy(evp_key, key, key_len);
    if (iv) {
      memcpy(evp_iv, iv, iv_len);      
    }
  
    int output_len = 0;
    int len = 0;
    unsigned char *buffer = NULL;
    
    EVP_CIPHER_CTX_init(&c);
    EVP_DecryptInit_ex(&c, type, NULL, evp_key, iv ? evp_iv : NULL);
    buffer = (unsigned char *)malloc(input_len + EVP_CIPHER_CTX_block_size(&c));
	EVP_DecryptUpdate(&c, buffer, &len, input, input_len);
    output_len += len;
	EVP_DecryptFinal(&c, &buffer[len], &len);
    output_len += len;
    
    lua_pushlstring(L, (char*) buffer, output_len);
    free(buffer);
    return 1;
  }
}

/*************** HMAC API ***************/

static HMAC_CTX *hmac_pnew(lua_State *L)
{
  HMAC_CTX *c = (HMAC_CTX*)lua_newuserdata(L, sizeof(HMAC_CTX));
  luaL_getmetatable(L, LUACRYPTO_HMACNAME);
  lua_setmetatable(L, -2);
  return c;
}

static int hmac_fnew(lua_State *L)
{
  HMAC_CTX *c = hmac_pnew(L);
  const char *s = luaL_checkstring(L, 1);
  const char *k = luaL_checkstring(L, 2);
  const EVP_MD *type = EVP_get_digestbyname(s);

  if (type == NULL) {
    luaL_argerror(L, 1, "invalid digest type");
    return 0;
  }

  HMAC_CTX_init(c);
  HMAC_Init_ex(c, k, lua_strlen(L, 2), type, NULL);

  return 1;
}

static int hmac_clone(lua_State *L)
{
 HMAC_CTX *c = (HMAC_CTX*)luaL_checkudata(L, 1, LUACRYPTO_HMACNAME);
 HMAC_CTX *d = hmac_pnew(L);
 *d = *c;
 return 1;
}

static int hmac_reset(lua_State *L)
{
  HMAC_CTX *c = (HMAC_CTX*)luaL_checkudata(L, 1, LUACRYPTO_HMACNAME);
  HMAC_Init_ex(c, NULL, 0, NULL, NULL);
  return 0;
}

static int hmac_update(lua_State *L)
{
  HMAC_CTX *c = (HMAC_CTX*)luaL_checkudata(L, 1, LUACRYPTO_HMACNAME);
  const char *s = luaL_checkstring(L, 2);

  HMAC_Update(c, (unsigned char *)s, lua_strlen(L, 2));

  lua_settop(L, 1);
  return 1;
}

static int hmac_final(lua_State *L)
{
  HMAC_CTX *c = (HMAC_CTX*)luaL_checkudata(L, 1, LUACRYPTO_HMACNAME);
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int written = 0;
  unsigned int i;
  char *hex;

  if (lua_isstring(L, 2))
  {
    const char *s = luaL_checkstring(L, 2);
    HMAC_Update(c, (unsigned char *)s, lua_strlen(L, 2));
  }

  HMAC_Final(c, digest, &written);

  if (lua_toboolean(L, 3))
    lua_pushlstring(L, (char *)digest, written);
  else
  {
    hex = (char*)calloc(sizeof(char), written*2 + 1);
    for (i = 0; i < written; i++)
      sprintf(hex + 2*i, "%02x", digest[i]);
    lua_pushlstring(L, hex, written*2);
    free(hex);
  }

  return 1;
}

static int hmac_tostring(lua_State *L)
{
  HMAC_CTX *c = (HMAC_CTX*)luaL_checkudata(L, 1, LUACRYPTO_HMACNAME);
  char s[64];
  sprintf(s, "%s %p", LUACRYPTO_HMACNAME, (void *)c);
  lua_pushstring(L, s);
  return 1;
}

static int hmac_gc(lua_State *L)
{
  HMAC_CTX *c = (HMAC_CTX*)luaL_checkudata(L, 1, LUACRYPTO_HMACNAME);
  HMAC_CTX_cleanup(c);
  return 1;
}

static int hmac_fdigest(lua_State *L)
{
  HMAC_CTX c;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int written = 0;
  unsigned int i;
  char *hex;
  const char *t = luaL_checkstring(L, 1);
  const char *s = luaL_checkstring(L, 2);
  const char *k = luaL_checkstring(L, 3);
  const EVP_MD *type = EVP_get_digestbyname(t);

  if (type == NULL) {
    luaL_argerror(L, 1, "invalid digest type");
    return 0;
  }

  HMAC_CTX_init(&c);
  HMAC_Init_ex(&c, k, lua_strlen(L, 3), type, NULL);
  HMAC_Update(&c, (unsigned char *)s, lua_strlen(L, 2));
  HMAC_Final(&c, digest, &written);

  if (lua_toboolean(L, 4))
    lua_pushlstring(L, (char *)digest, written);
  else
  {
    hex = (char*)calloc(sizeof(char), written*2 + 1);
    for (i = 0; i < written; i++)
      sprintf(hex + 2*i, "%02x", digest[i]);
    lua_pushlstring(L, hex, written*2);
    free(hex);
  }

  return 1;
}

/*************** SIGN API ***************/

static EVP_MD_CTX *sign_pnew(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)lua_newuserdata(L, sizeof(EVP_MD_CTX));
  luaL_getmetatable(L, LUACRYPTO_SIGNNAME);
  lua_setmetatable(L, -2);
  return c;
}

static int sign_fnew(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  const EVP_MD *md = EVP_get_digestbyname(s);
  if (md == NULL) {
    luaL_argerror(L, 1, "invalid digest type");
    return 0;
  } else {
    EVP_MD_CTX *c = sign_pnew(L);
    EVP_MD_CTX_init(c);
    EVP_SignInit_ex(c, md, NULL);
    return 1;
  }
}

static int sign_update(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_SIGNNAME);
  size_t input_len = 0;
  const unsigned char *input = (unsigned char *) luaL_checklstring(L, 2, &input_len);

  EVP_SignUpdate(c, input, input_len);
  return 0;
}

static int sign_final(lua_State *L) 
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_SIGNNAME);
  unsigned int output_len = 0;
  unsigned char *buffer;
  EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 2, LUACRYPTO_PKEYNAME);
  
  buffer = (unsigned char*)malloc(EVP_PKEY_size(*pkey));
  if (!EVP_SignFinal(c, buffer, &output_len, *pkey)) {
    free(buffer);
    return crypto_error(L);
  }
  lua_pushlstring(L, (char*) buffer, output_len);
  free(buffer);
  
  return 1;
}

static int sign_tostring(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_SIGNNAME);
  char s[64];
  sprintf(s, "%s %p", LUACRYPTO_SIGNNAME, (void *)c);
  lua_pushstring(L, s);
  return 1;
}

static int sign_gc(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_SIGNNAME);
  EVP_MD_CTX_cleanup(c);
  return 1;
}

static int sign_fsign(lua_State *L)
{
  /* parameter 1 is the 'crypto.sign' table */
  const char *type_name = luaL_checkstring(L, 2);
  const EVP_MD *type = EVP_get_digestbyname(type_name);

  if (type == NULL) {
    luaL_argerror(L, 2, "invalid digest type");
    return 0;
  } else {
    EVP_MD_CTX c;
    size_t input_len = 0;
    const unsigned char *input = (unsigned char *) luaL_checklstring(L, 3, &input_len);
    unsigned int output_len = 0;
    unsigned char *buffer = NULL;
    EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 4, LUACRYPTO_PKEYNAME);
   
    EVP_MD_CTX_init(&c);
    EVP_SignInit_ex(&c, type, NULL);
    buffer = (unsigned char*)malloc(EVP_PKEY_size(*pkey));
    EVP_SignUpdate(&c, input, input_len);
    if (!EVP_SignFinal(&c, buffer, &output_len, *pkey)) {
      free(buffer);
      return crypto_error(L);
    }

    lua_pushlstring(L, (char*) buffer, output_len);
    free(buffer);
    return 1;
  }
}

/*************** VERIFY API ***************/

static EVP_MD_CTX *verify_pnew(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)lua_newuserdata(L, sizeof(EVP_MD_CTX));
  luaL_getmetatable(L, LUACRYPTO_VERIFYNAME);
  lua_setmetatable(L, -2);
  return c;
}

static int verify_fnew(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  const EVP_MD *md = EVP_get_digestbyname(s);
  if (md == NULL) {
    luaL_argerror(L, 1, "invalid digest type");
    return 0;
  } else {
    EVP_MD_CTX *c = verify_pnew(L);
    EVP_MD_CTX_init(c);
    EVP_VerifyInit_ex(c, md, NULL);
    return 1;
  }
}

static int verify_update(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_VERIFYNAME);
  size_t input_len = 0;
  const unsigned char *input = (unsigned char *) luaL_checklstring(L, 2, &input_len);

  EVP_VerifyUpdate(c, input, input_len);
  return 0;
}

static int verify_final(lua_State *L) 
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_VERIFYNAME);
  size_t sig_len = 0;
  const unsigned char *sig = (unsigned char *) luaL_checklstring(L, 2, &sig_len);
  EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 3, LUACRYPTO_PKEYNAME);
  int ret;

  ret = EVP_VerifyFinal(c, sig, sig_len, *pkey);
  if (ret == -1)
    return crypto_error(L);

  lua_pushboolean(L, ret);  
  return 1;
}

static int verify_tostring(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_VERIFYNAME);
  char s[64];
  sprintf(s, "%s %p", LUACRYPTO_VERIFYNAME, (void *)c);
  lua_pushstring(L, s);
  return 1;
}

static int verify_gc(lua_State *L)
{
  EVP_MD_CTX *c = (EVP_MD_CTX*)luaL_checkudata(L, 1, LUACRYPTO_VERIFYNAME);
  EVP_MD_CTX_cleanup(c);
  return 1;
}

static int verify_fverify(lua_State *L)
{
  /* parameter 1 is the 'crypto.verify' table */
  const char *type_name = luaL_checkstring(L, 2);
  const EVP_MD *type = EVP_get_digestbyname(type_name);

  if (type == NULL) {
    luaL_argerror(L, 1, "invalid digest type");
    return 0;
  } else {
    EVP_MD_CTX c;
    size_t input_len = 0;
    const unsigned char *input = (unsigned char *) luaL_checklstring(L, 3, &input_len);
    size_t sig_len = 0;
    const unsigned char *sig = (unsigned char *) luaL_checklstring(L, 4, &sig_len);
    EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 5, LUACRYPTO_PKEYNAME);
    int ret;

    EVP_MD_CTX_init(&c);
    EVP_VerifyInit_ex(&c, type, NULL);
    EVP_VerifyUpdate(&c, input, input_len);
    ret = EVP_VerifyFinal(&c, sig, sig_len, *pkey);
    if (ret == -1)
      return crypto_error(L);

    lua_pushboolean(L, ret);
    return 1;
  }
}
/*************** RAND API ***************/

static int rand_do_bytes(lua_State *L, int (*bytes)(unsigned char *, int))
{
  size_t count = luaL_checkint(L, 1);
  unsigned char tmp[256], *buf = tmp;
  if (count > sizeof tmp)
    buf = (unsigned char*)malloc(count);
    if (!buf)
      return luaL_error(L, "out of memory");
    else if (!bytes(buf, count))
      return crypto_error(L);
    lua_pushlstring(L, (char *)buf, count);
    if (buf != tmp)
      free(buf);
    return 1;
}

static int rand_bytes(lua_State *L)
{
  return rand_do_bytes(L, RAND_bytes);
}

static int rand_pseudo_bytes(lua_State *L)
{
  return rand_do_bytes(L, RAND_pseudo_bytes);
}

static int rand_add(lua_State *L)
{
  size_t num;
  const void *buf = luaL_checklstring(L, 1, &num);
  double entropy = luaL_optnumber(L, 2, num);
  RAND_add(buf, num, entropy);
  return 0;
}

static int rand_status(lua_State *L)
{
  lua_pushboolean(L, RAND_status());
  return 1;
}

enum { WRITE_FILE_COUNT = 1024 };
static int rand_load(lua_State *L)
{
  const char *name = luaL_optstring(L, 1, 0);
  char tmp[256];
  int n;
  if (!name && !(name = RAND_file_name(tmp, sizeof tmp)))
    return crypto_error(L);
  n = RAND_load_file(name, WRITE_FILE_COUNT);
  if (n == 0)
    return crypto_error(L);
  lua_pushnumber(L, n);
  return 1;
}

static int rand_write(lua_State *L)
{
  const char *name = luaL_optstring(L, 1, 0);
  char tmp[256];
  int n;
  if (!name && !(name = RAND_file_name(tmp, sizeof tmp)))
    return crypto_error(L);
  n = RAND_write_file(name);
  if (n == 0)
    return crypto_error(L);
  lua_pushnumber(L, n);
  return 1;
}

static int rand_cleanup(lua_State *L)
{
  RAND_cleanup();
  return 0;
}

/*************** PKEY API ***************/

static EVP_PKEY **pkey_new(lua_State *L)
{
  EVP_PKEY **pkey = (EVP_PKEY **)lua_newuserdata(L, sizeof(EVP_PKEY*));
  luaL_getmetatable(L, LUACRYPTO_PKEYNAME);
  lua_setmetatable(L, -2);
  return pkey;
}
  
static int pkey_generate(lua_State *L)
{
  const char *options[] = {"rsa", "dsa"};
  int idx = luaL_checkoption(L, 1, NULL, options);
  int key_len = luaL_checkinteger(L, 2);
  EVP_PKEY **pkey = pkey_new(L);
  if (idx==0) {
    RSA *rsa = RSA_generate_key(key_len, RSA_F4, NULL, NULL);
    if (!rsa)
      return crypto_error(L);
    
    *pkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(*pkey, rsa);
    return 1;
  } else {
    DSA *dsa = DSA_generate_parameters(key_len, NULL, 0, NULL, NULL, NULL, NULL);
    if (!DSA_generate_key(dsa))
      return crypto_error(L);
    
    *pkey = EVP_PKEY_new();
    EVP_PKEY_assign_DSA(*pkey, dsa);
    return 1;
  }
}

static int pkey_read(lua_State *L)
{
  const char *filename = luaL_checkstring(L, 1);
  int readPrivate = lua_isboolean(L, 2) && lua_toboolean(L, 2);
  FILE *fp = fopen(filename, "r");
  EVP_PKEY **pkey = pkey_new(L);
  
  if (!fp)
    luaL_error(L, "File not found: %s", filename);
  
  if (readPrivate) {
    *pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
  } else {
    *pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
  }
  
  fclose(fp);
  
  if (! *pkey)
    return crypto_error(L);
  
  return 1;
}

static int pkey_write(lua_State *L)
{
  EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 1, LUACRYPTO_PKEYNAME);
  const char *pubfn = lua_tostring(L, 2);
  const char *privfn = lua_tostring(L, 3);
  if (pubfn) {
    FILE *fp = fopen(pubfn, "w");
    if (!fp)
      luaL_error(L, "Unable to write to file: %s", pubfn);
    if (!PEM_write_PUBKEY(fp, *pkey))
      return crypto_error(L);
    fclose(fp);
  }
  if (privfn) {
    FILE *fp = fopen(privfn, "w");
    if (!fp)
      luaL_error(L, "Unable to write to file: %s", privfn);
    if (!PEM_write_PrivateKey(fp, *pkey, NULL, NULL, 0, NULL, NULL))
      return crypto_error(L);
    fclose(fp);  
  }
  return 0;
}

static int pkey_gc(lua_State *L)
{
  EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 1, LUACRYPTO_PKEYNAME);
  EVP_PKEY_free(*pkey);
  return 0;
}
  
static int pkey_tostring(lua_State *L)
{
  EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 1, LUACRYPTO_PKEYNAME);
  char buf[60];
  sprintf(buf, "%s %s %d %p", LUACRYPTO_PKEYNAME, (*pkey)->type == EVP_PKEY_DSA ? "DSA" : "RSA", EVP_PKEY_bits(*pkey), pkey);
  lua_pushstring(L, buf);
  return 1;
}

/*************** SEAL API ***************/

typedef struct seal_context {
	EVP_CIPHER_CTX* ctx;
	int eklen;
	unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char *ek;
} seal_context;

static seal_context *seal_pnew(lua_State* L)
{
	seal_context *c = (seal_context*)lua_newuserdata(L, sizeof(seal_context));
	luaL_getmetatable(L, LUACRYPTO_SEALNAME);
	lua_setmetatable(L, -2);

	memset(c, 0, sizeof(seal_context));
	c->ctx = (EVP_CIPHER_CTX*)malloc(sizeof(EVP_CIPHER_CTX));
	
	return c;
}

static int seal_gc(lua_State* L)
{
	seal_context *c = (seal_context*)luaL_checkudata(L, 1, LUACRYPTO_SEALNAME);
	EVP_CIPHER_CTX_cleanup(c->ctx);
	free(c->ctx);
	if(c->ek != NULL) {
		free(c->ek);
	}
	return 0;
}

static int seal_tostring(lua_State* L)
{
	seal_context *c = (seal_context*)luaL_checkudata(L, 1, LUACRYPTO_SEALNAME);
	char s[64];
	sprintf(s, "%s %p %s", LUACRYPTO_SEALNAME, (void *)c, EVP_CIPHER_name(c->ctx->cipher));
	lua_pushstring(L, s);

	return 1;
}

static int seal_fnew(lua_State* L)
{
	const char *cipher_type = luaL_checkstring(L, 1);
	const EVP_CIPHER *cipher = EVP_get_cipherbyname(cipher_type);
	if (cipher == NULL) {
		luaL_argerror(L, 1, "invalid encrypt cipher");
		return 0;
	}
	
	EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 2, LUACRYPTO_PKEYNAME);

	int npubk = 1;

	seal_context *seal_ctx = seal_pnew(L);
	EVP_CIPHER_CTX_init(seal_ctx->ctx);

	seal_ctx->ek = (unsigned char*)malloc(EVP_PKEY_size(*pkey) * npubk);

	if(!EVP_SealInit(seal_ctx->ctx, cipher, &seal_ctx->ek, &seal_ctx->eklen, seal_ctx->iv, pkey, npubk))
	{
		free(seal_ctx->ek);
		seal_ctx->ek = NULL;
		return crypto_error(L);
	}

	return 1;
}

static int seal_update(lua_State* L)
{
	seal_context *c = (seal_context*)luaL_checkudata(L, 1, LUACRYPTO_SEALNAME);
	size_t input_len = 0;
	const unsigned char *input = (unsigned char *) luaL_checklstring(L, 2, &input_len);
	int output_len = 0; 

	luaL_Buffer buffer;
	luaL_buffinit(L, &buffer);

	if(c->ek != NULL) {
		char* t = luaL_prepbuffer(&buffer);
		memcpy(t, &c->eklen, sizeof(c->eklen));
		luaL_addsize(&buffer, sizeof(c->eklen));

		luaL_addlstring(&buffer, (const char*)c->ek, c->eklen);
		luaL_addlstring(&buffer, (const char*)c->iv, EVP_CIPHER_iv_length(c->ctx->cipher));
		free(c->ek);
		c->ek = NULL;
	}

	unsigned char *temp = (unsigned char*)malloc(input_len + EVP_CIPHER_CTX_block_size(c->ctx));
	EVP_SealUpdate(c->ctx, temp, &output_len, input, input_len);
	luaL_addlstring(&buffer, (char*) temp, output_len);
	free(temp);

	luaL_pushresult(&buffer);
	
	return 1;
}

static int seal_final(lua_State* L)
{
	seal_context *c = (seal_context*)luaL_checkudata(L, 1, LUACRYPTO_SEALNAME);
	int output_len = 0;
	unsigned char buffer[EVP_MAX_BLOCK_LENGTH];

	EVP_SealFinal(c->ctx, buffer, &output_len);
	lua_pushlstring(L, (char*) buffer, output_len);

	return 1;
}

static int seal_fseal(lua_State* L)
{
	/* parameter 1 is the 'crypto.seal' table */
	const char *cipher_type = luaL_checkstring(L, 2);
	const EVP_CIPHER *cipher = EVP_get_cipherbyname(cipher_type);
	if (cipher == NULL) {
		luaL_argerror(L, 1, "invalid encrypt cipher");
		return 0;
	}

	int npubk = 1;
	int eklen;
	unsigned char iv[EVP_MAX_IV_LENGTH];

	const unsigned char* message = (const unsigned char*)luaL_checkstring(L, 3);
	int message_length = lua_objlen(L, 3);
	
	EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 4, LUACRYPTO_PKEYNAME);

	unsigned char *ek = (unsigned char*)malloc(EVP_PKEY_size(*pkey) * npubk);

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);

	if(!EVP_SealInit(&ctx, cipher, &ek, &eklen, iv, pkey, npubk))
	{
		free(ek);
		EVP_CIPHER_CTX_cleanup(&ctx);
		return crypto_error(L);
	}

	luaL_Buffer buffer;
	luaL_buffinit(L, &buffer);

	char* t = luaL_prepbuffer(&buffer);
	memcpy(t, &eklen, sizeof(eklen));
	luaL_addsize(&buffer, sizeof(eklen));

	luaL_addlstring(&buffer, (const char*)ek, eklen);
	luaL_addlstring(&buffer, (const char*)iv, EVP_CIPHER_iv_length(cipher));

	// EVP_aes_128_cbc()

	int block_size = EVP_CIPHER_block_size(cipher);

	while(message_length > 0) {
		char* temp = luaL_prepbuffer(&buffer);
		int sz = min(LUAL_BUFFERSIZE - block_size - 1, message_length);
		int output_length;

		if(!EVP_SealUpdate(&ctx, (unsigned char*)temp, &output_length, message, sz)) {
			free(ek);
			EVP_CIPHER_CTX_cleanup(&ctx);
			return crypto_error(L);
		}
		message += sz;
		message_length -= sz;
		luaL_addsize(&buffer, output_length);
	}

	int output_length;
	char *temp = luaL_prepbuffer(&buffer);
	if(!EVP_SealFinal(&ctx, (unsigned char*)temp, &output_length))
	{
		free(ek);
		EVP_CIPHER_CTX_cleanup(&ctx);
		return crypto_error(L);
	}

	luaL_addsize(&buffer, output_length);

	luaL_pushresult(&buffer);
	EVP_CIPHER_CTX_cleanup(&ctx);
	free(ek);
	return 1;
}

/*************** OPEN API ***************/

typedef struct open_context {
	EVP_CIPHER_CTX* ctx;
	EVP_CIPHER* cipher;
	int pkey_ref;
	int first;
} open_context;

static open_context *open_pnew(lua_State* L)
{
	open_context *c = (open_context*)lua_newuserdata(L, sizeof(open_context));
	luaL_getmetatable(L, LUACRYPTO_OPENNAME);
	lua_setmetatable(L, -2);
	
	memset(c, 0, sizeof(open_context));
	c->ctx = (EVP_CIPHER_CTX*)malloc(sizeof(EVP_CIPHER_CTX));
	c->first = 1;

	return c;
}

static int open_gc(lua_State* L)
{
	open_context *c = (open_context*)luaL_checkudata(L, 1, LUACRYPTO_OPENNAME);
	EVP_CIPHER_CTX_cleanup(c->ctx);
	free(c->ctx);
	if(c->pkey_ref != LUA_NOREF) {
		luaL_unref(L, LUA_REGISTRYINDEX, c->pkey_ref);
	}
	return 0;
}

static int open_tostring(lua_State* L)
{
	open_context *c = (open_context*)luaL_checkudata(L, 1, LUACRYPTO_OPENNAME);

	lua_rawgeti(L, LUA_REGISTRYINDEX, c->pkey_ref);
	EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, -1, LUACRYPTO_PKEYNAME);

	char s[64];
	sprintf(s, "%s %p %s %s %d %p", LUACRYPTO_OPENNAME, (void *)c, EVP_CIPHER_name(c->cipher),
		(*pkey)->type == EVP_PKEY_DSA ? "DSA" : "RSA", EVP_PKEY_bits(*pkey), pkey);
	
	lua_pop(L, 1);
	lua_pushstring(L, s);
	return 1;
}

static int open_fnew(lua_State* L)
{
	const char *type_name = luaL_checkstring(L, 1);
	const EVP_CIPHER *cipher = EVP_get_cipherbyname(type_name);
	if (cipher == NULL) {
		luaL_argerror(L, 1, "invalid decrypt cipher");
		return 0;
	}

	EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 2, LUACRYPTO_PKEYNAME);

	open_context *open_ctx = open_pnew(L);
	EVP_CIPHER_CTX_init(open_ctx->ctx);
	open_ctx->cipher = (EVP_CIPHER*)cipher;

	lua_pushvalue(L, 2);
	open_ctx->pkey_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	return 1;
}

static int open_update(lua_State* L)
{
	open_context *c = (open_context*)luaL_checkudata(L, 1, LUACRYPTO_OPENNAME);
	size_t input_len = 0;
	const unsigned char *input = (unsigned char *) luaL_checklstring(L, 2, &input_len);
	int output_len = 0;

	if(c->first) {
		int eklen;
		unsigned char iv[EVP_MAX_IV_LENGTH];

		c->first = 0;

		memcpy(&eklen, input, sizeof(eklen));
		input += sizeof(eklen);
		input_len -= sizeof(eklen);

		unsigned char* encrypted_key = (unsigned char*)malloc(eklen);
		memcpy(encrypted_key, input, eklen);
		input += eklen;
		input_len -= eklen;

		int cipher_length = EVP_CIPHER_iv_length(c->cipher);
		memcpy(&iv, input, cipher_length);
		input += cipher_length;
		input_len -= cipher_length;

		lua_rawgeti(L, LUA_REGISTRYINDEX, c->pkey_ref);
		EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, -1, LUACRYPTO_PKEYNAME);

		if(!EVP_OpenInit(c->ctx, c->cipher, encrypted_key, eklen, iv, *pkey))
		{
			free(encrypted_key);
			luaL_unref(L, LUA_REGISTRYINDEX, c->pkey_ref);
			c->pkey_ref = LUA_NOREF;
			return crypto_error(L);
		}
		luaL_unref(L, LUA_REGISTRYINDEX, c->pkey_ref);
		c->pkey_ref = LUA_NOREF;
	}

	/*	*/

	luaL_Buffer buffer;
	luaL_buffinit(L, &buffer);

	while(input_len > 0) {
		int output_length;
		unsigned char* temp = (unsigned char*)luaL_prepbuffer(&buffer);
		size_t sz = min(LUAL_BUFFERSIZE - 1, input_len);
		if(!EVP_OpenUpdate(c->ctx, temp, &output_length, input, sz)) {
			return crypto_error(L);
		}

		input += sz;
		input_len -= sz;
		luaL_addsize(&buffer, output_length);
	}

	luaL_pushresult(&buffer);
	return 1;

	/*int output_length;
	unsigned char *temp = (unsigned char*)luaL_prepbuffer(&buffer);
	if(!EVP_OpenFinal(&ctx, temp, &output_length))
	{
		free(encrypted_key);
		EVP_CIPHER_CTX_cleanup(&ctx);
		return crypto_error(L);
	}
	luaL_addsize(&buffer, output_length);

	luaL_pushresult(&buffer);
	free(encrypted_key);
	EVP_CIPHER_CTX_cleanup(&ctx);
	return 1;*/
}

static int open_final(lua_State* L)
{
	open_context *c = (open_context*)luaL_checkudata(L, 1, LUACRYPTO_OPENNAME);
	int output_len = 0;
	unsigned char buffer[EVP_MAX_BLOCK_LENGTH];

	EVP_OpenFinal(c->ctx, buffer, &output_len);
	lua_pushlstring(L, (char*) buffer, output_len);

	return 1;
}

static int open_fopen(lua_State* L)
{
	/* parameter 1 is the 'crypto.open' table */
	const char *type_name = luaL_checkstring(L, 2);
	const EVP_CIPHER *cipher = EVP_get_cipherbyname(type_name);
	if (cipher == NULL) {
		luaL_argerror(L, 1, "invalid decrypt cipher");
		return 0;
	}

	unsigned char* data = (unsigned char*)luaL_checkstring(L, 3);
	int data_length = lua_objlen(L, 3);

	EVP_PKEY **pkey = (EVP_PKEY **)luaL_checkudata(L, 4, LUACRYPTO_PKEYNAME);

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);

	int eklen;
	unsigned char iv[EVP_MAX_IV_LENGTH];

	memcpy(&eklen, data, sizeof(eklen));
	data += sizeof(eklen);
	data_length -= sizeof(eklen);

	unsigned char* encrypted_key = (unsigned char*)malloc(eklen);
	memcpy(encrypted_key, data, eklen);
	data += eklen;
	data_length -= eklen;

	int cipher_length = EVP_CIPHER_iv_length(cipher);
	memcpy(&iv, data, cipher_length);
	data += cipher_length;
	data_length -= cipher_length;

	if(!EVP_OpenInit(&ctx, cipher, encrypted_key, eklen, iv, *pkey))
	{
		free(encrypted_key);
		EVP_CIPHER_CTX_cleanup(&ctx);
		return crypto_error(L);
	}

	luaL_Buffer buffer;
	luaL_buffinit(L, &buffer);

	while(data_length > 0) {
		int output_length;
		unsigned char* temp = (unsigned char*)luaL_prepbuffer(&buffer);
		size_t sz = min(LUAL_BUFFERSIZE - 1, data_length);
		if(!EVP_OpenUpdate(&ctx, temp, &output_length, data, sz)) {
			free(encrypted_key);
			EVP_CIPHER_CTX_cleanup(&ctx);
			return crypto_error(L);
		}

		data += sz;
		data_length -= sz;
		luaL_addsize(&buffer, output_length);
	}

	int output_length;
	unsigned char *temp = (unsigned char*)luaL_prepbuffer(&buffer);
	if(!EVP_OpenFinal(&ctx, temp, &output_length))
	{
		free(encrypted_key);
		EVP_CIPHER_CTX_cleanup(&ctx);
		return crypto_error(L);
	}
	luaL_addsize(&buffer, output_length);

	luaL_pushresult(&buffer);
	free(encrypted_key);
	EVP_CIPHER_CTX_cleanup(&ctx);
	return 1;
}

/*************** CORE API ***************/
  
static void list_callback(const OBJ_NAME *obj,void *arg) {
  lua_State *L = (lua_State*) arg;
  int idx = lua_objlen(L, -1);
  lua_pushstring(L, obj->name);
  lua_rawseti(L, -2, idx + 1);
}

static int luacrypto_list(lua_State *L) {
  int options[] = {OBJ_NAME_TYPE_CIPHER_METH, OBJ_NAME_TYPE_MD_METH};
  const char * names[] = {"ciphers", "digests"};
  int idx = luaL_checkoption (L, 1, NULL, names);
  lua_createtable(L, 0, 0);
  OBJ_NAME_do_all_sorted(options[idx], list_callback, L);
  return 1;
}

static int luacrypto_hex(lua_State *L) {
  size_t i, len = 0;
  const unsigned char * input = (unsigned char *) luaL_checklstring(L, 1, &len);
  char * hex = (char*)calloc(sizeof(char), len*2 + 1);
  for (i = 0; i < len; i++) {
    sprintf(hex + 2*i, "%02x", input[i]);
  }
  lua_pushlstring(L, hex, len*2);
  free(hex);
  return 1;
}
  
/*
** Create a metatable and leave it on top of the stack.
*/
LUACRYPTO_API int luacrypto_createmeta (lua_State *L, const char *name, const luaL_reg *methods) {
  if (!luaL_newmetatable (L, name))
    return 0;
  
  /* define methods */
  luaL_openlib (L, NULL, methods, 0);
  
  /* define metamethods */
  lua_pushliteral (L, "__index");
  lua_pushvalue (L, -2);
  lua_settable (L, -3);

  lua_pushliteral (L, "__metatable");
  lua_pushliteral (L, LUACRYPTO_PREFIX"you're not allowed to get this metatable");
  lua_settable (L, -3);

  return 1;
}
  


static void create_call_table(lua_State *L, const char *name, lua_CFunction creator, lua_CFunction starter)
{
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, creator);
  lua_setfield(L, -2, "new");
  /* create metatable for call */
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, starter);
  lua_setfield(L, -2, "__call");
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, name);
}

#define EVP_METHODS(name) \
  struct luaL_reg name##_methods[] = {  \
    { "__tostring", name##_tostring },  \
    { "__gc", name##_gc },              \
    { "final", name##_final },          \
    { "tostring", name##_tostring },    \
    { "update", name##_update },        \
    {NULL, NULL},                       \
  }

/*
** Create metatables for each class of object.
*/
static void create_metatables (lua_State *L)
{
  struct luaL_reg core_functions[] = {
    { "list", luacrypto_list },
    { "hex", luacrypto_hex },
    { NULL, NULL }
  };
  struct luaL_reg digest_methods[] = {
    { "__tostring", digest_tostring },
    { "__gc", digest_gc },
    { "final", digest_final },
    { "tostring", digest_tostring },
    { "update", digest_update },
    { "reset", digest_reset },
    { "clone", digest_clone },
    {NULL, NULL}
  };
  EVP_METHODS(encrypt);
  EVP_METHODS(decrypt);
  EVP_METHODS(sign);
  EVP_METHODS(verify);
  EVP_METHODS(seal);
  EVP_METHODS(open);  
  struct luaL_reg hmac_functions[] = {
    { "digest", hmac_fdigest },
    { "new", hmac_fnew },
    { NULL, NULL }
  };
  struct luaL_reg hmac_methods[] = {
    { "__tostring", hmac_tostring },
    { "__gc", hmac_gc },
    { "clone", hmac_clone },
    { "final", hmac_final },
    { "reset", hmac_reset },
    { "tostring", hmac_tostring },
    { "update", hmac_update },
    { NULL, NULL }
  };
  struct luaL_reg rand_functions[] = {
    { "bytes", rand_bytes },
    { "pseudo_bytes", rand_pseudo_bytes },
    { "add", rand_add },
    { "seed", rand_add },
    { "status", rand_status },
    { "load", rand_load },
    { "write", rand_write },
    { "cleanup", rand_cleanup },
    { NULL, NULL }
  };
  struct luaL_reg pkey_functions[] = {
    { "generate", pkey_generate },
    { "read", pkey_read },
    { NULL, NULL }
  };
  struct luaL_reg pkey_methods[] = {
    { "__tostring", pkey_tostring },
    { "__gc", pkey_gc },
    { "write", pkey_write },
    { NULL, NULL }
  };
  
  luaL_register (L, LUACRYPTO_CORENAME, core_functions);
#define CALLTABLE(n) create_call_table(L, #n, n##_fnew, n##_f##n)
  CALLTABLE(digest);
  CALLTABLE(encrypt);
  CALLTABLE(decrypt);
  CALLTABLE(verify);
  CALLTABLE(sign);
  CALLTABLE(seal);
  CALLTABLE(open);

  luacrypto_createmeta(L, LUACRYPTO_DIGESTNAME, digest_methods);
  luacrypto_createmeta(L, LUACRYPTO_ENCRYPTNAME, encrypt_methods);
  luacrypto_createmeta(L, LUACRYPTO_DECRYPTNAME, decrypt_methods);
  luacrypto_createmeta(L, LUACRYPTO_HMACNAME, hmac_methods);
  luacrypto_createmeta(L, LUACRYPTO_SIGNNAME, sign_methods);
  luacrypto_createmeta(L, LUACRYPTO_VERIFYNAME, verify_methods);
  luacrypto_createmeta(L, LUACRYPTO_PKEYNAME, pkey_methods);
  luacrypto_createmeta(L, LUACRYPTO_SEALNAME, seal_methods);
  luacrypto_createmeta(L, LUACRYPTO_OPENNAME, open_methods);

  luaL_register (L, LUACRYPTO_RANDNAME, rand_functions);
  luaL_register (L, LUACRYPTO_HMACNAME, hmac_functions);
  luaL_register (L, LUACRYPTO_PKEYNAME, pkey_functions);
  
  lua_pop (L, 3);
}

/*
** Define the metatable for the object on top of the stack
*/
LUACRYPTO_API void luacrypto_setmeta (lua_State *L, const char *name) {
  luaL_getmetatable (L, name);
  lua_setmetatable (L, -2);
}

/*
** Assumes the table is on top of the stack.
*/
LUACRYPTO_API void luacrypto_set_info (lua_State *L) {
  lua_pushliteral (L, "_COPYRIGHT");
  lua_pushliteral (L, "Copyright (C) 2005-2006 Keith Howe");
  lua_settable (L, -3);
  lua_pushliteral (L, "_DESCRIPTION");
  lua_pushliteral (L, "LuaCrypto is a Lua wrapper for OpenSSL");
  lua_settable (L, -3);
  lua_pushliteral (L, "_VERSION");
  lua_pushliteral (L, "LuaCrypto 0.2.0");
  lua_settable (L, -3);
}

/*
** Creates the metatables for the objects and registers the
** driver open method.
*/
#ifdef __cplusplus
extern "C"
#endif
LUACRYPTO_API int luaopen_crypto(lua_State *L)
{
  OpenSSL_add_all_digests();
  OpenSSL_add_all_ciphers();
  
  struct luaL_reg core[] = {
    {NULL, NULL},
  };
  create_metatables (L);
  luaL_openlib (L, LUACRYPTO_CORENAME, core, 0);
  luacrypto_set_info (L);
  return 1;
}
