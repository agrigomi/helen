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
* Configuring virtual hosts.

In your home folder, create a directory called `test` for example. It should contain the file `http.json`. The contents of `test/http.json` look like this:
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

