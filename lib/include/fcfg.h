#ifndef __FCFG_H__
#define __FCFG_H__

#include <string>
#include <map>
#include "err.h"

#define EOF_SIGNATURE		"--eof--\n"

typedef std::map<std::string, std::string> _fcfg_map_t;
typedef std::pair<std::string, std::string> _fcfg_pair_t;

_err_t fcfg_read(FILE *stream, _fcfg_map_t &map, const char div=':', bool enable_comments=true);
_err_t fcfg_read(const char *fname, _fcfg_map_t &map, const char div=':', bool enable_comments=true);
_err_t fcfg_write(FILE *stream, _fcfg_map_t &map, const char div=':');
_err_t fcfg_write_eof(FILE *stream);
_err_t fcfg_write(const char *fname, _fcfg_map_t &map, const char div=':');


#endif
