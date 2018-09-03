#include "json.h"
#include "strmap.h"
#include <stdio.h>
#include <stdbool.h>

static const char JSON_ARRAY_TEMPLATE[] = "[ %s ]";
static const char JSON_OBJECT_TEMPLATE[] = "{ %s }";
static const char JSON_KEYVAL_TEMPLATE[] = "%s: %s";
static const char JSON_DQUOTE_TEMPLATE[] = "\"%s\"";
static const char JSON_SQUOTE_TEMPLATE[] = "\'%s\'";

void StrMap_to_json(StrMap* sm, char* buf, size_t max_len)
{
    sm_enum(sm, enum_cb, NULL);
}

static enum_cb(const char* key, const char* value, const char* obj)
{

}

bool json_to_StrMap(const char* buf, size_t numOfKeys)
{
    StrMap* sm = sm_new(numOfKeys);

}

void list_to_json(char* buf, size_t max_len)
{
    
}