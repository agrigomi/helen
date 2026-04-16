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