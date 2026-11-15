# Helen is a lite and flexible HTTP server
### It supports:
* HTTP/1.1 (for now)
* SSL
* Virtual hosts
* gzip and deflate compression
* Ranges
* Proxy requests (danger because of anonimus for now)
* url and error handling
* JSON configurable

### Dependencies:
* libssl-dev
* libz-dev

### How to get
~~~
git clone https://github.com/agrigomi/helen.git
~~~

### How to build
~~~
cd helen
./configure
make
~~~

### How to configure
The configuration is stored in JSON files with fixed names `http.json` and `mapping.json`.
The JSON files (`http.json` and `mapping.json`) are the source files.
Since it is stupid to parse JSON files every time, they are `compiled` into .dat (hash files) optimized for fast searching.

* Virtual hosts.

In your home folder or somewhere else, create a new directory called for example `http_cfg`. 
It should contain the file `http.json`. The contents of `http_cfg/http.json` look like this:
```
"http": {
	"default": {
		"root": "<full path to documents root>"
	},

	"vhost": [
		{
			"host":	"www.example.com",
			"root": "<full path to documents root>"
		},
		{
			"host":	"www.example.net",
			"root": "<full path to documents root>"
		}
	]
}
```

* Mapping

The mapping file `mapping.json` should be located in the document root directory and is used for URL handling, error handling, and file extension mapping.
`<document root>/mapping.json`

The structure of `mapping.json` is:
```json
{
	"mapping": {
		"url": [
			{
				"method": "GET",
				"url": "/api/test",
				"protocol": "http",
				"header": true,
				"no-stderr": false,
				"input": false,
				"response-code": 200,
				"exec": "/path/to/cgi-bin/script",
				"response": "Alternative response text if not exec",
				"header-append": [
					"X-Custom-Header: value",
					"Cache-Control: no-cache"
				],
				"ext": ".html"
			}
		],
		"err": [
			{
				"code": 404,
				"header": true,
				"no-stderr": true,
				"input": false,
				"exec": "/path/to/cgi-bin/error_handler",
				"response": "Custom 404 Error Message",
				"header-append": [
					"X-Error-Reason: Not Found"
				],
				"ext": ".txt"
			}
		],
		"ext": [
			{
				"ident": ".php",
				"compression": "gzip",
				"header-append": [
					"X-Powered-By: Helen"
				]
			}
		]
	}
}
```

### Mapping tags description:

#### URL mapping (`url` array):
* `method`: HTTP method (GET, POST, etc.)
* `url`: The URL pattern to match.
* `protocol`: Protocol string.
* `header`: Boolean, whether to include headers.
* `no-stderr`: Boolean, suppress stderr for executed processes.
* `input`: Boolean, whether to provide input to the executed process.
* `response-code`: Forced HTTP response code.
* `exec`: Path to executable (CGI) for this URL.
* `response`: Static response content (used if `exec` is not provided).
* `header-append`: Array of strings to append to HTTP headers.
* `ext`: Extension hint.

#### Error mapping (`err` array):
* `code`: HTTP error code (e.g., 404, 500).
* `header`: Boolean, include headers in error response.
* `no-stderr`: Boolean, suppress stderr for error handler.
* `input`: Boolean, provide input to error handler.
* `exec`: Path to error handler executable.
* `response`: Static error response content.
* `header-append`: Array of headers to append.
* `ext`: Extension hint.

#### Extension mapping (`ext` array):
* `ident`: File extension (e.g., ".html", ".php").
* `compression`: Compression type (e.g., "gzip", "deflate").
* `header-append`: Array of headers to append for this extension.
