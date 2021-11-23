all: test-go test-go-pie

test-go: hello.go
	go build -o $@ $<

test-go-pie: hello.go
	go build -o $@ -buildmode=pie -ldflags -extldflags=-Wl,-z,now,-z,relro $<

