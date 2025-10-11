module github.com/as-family/zdb

go 1.24.1

require 6.5840 v0.0.0

require (
	github.com/anishathalye/porcupine v1.0.3 // indirect
	google.golang.org/protobuf v1.36.8 // indirect
)

replace 6.5840 => /home/ahmed/ws/zdb/6.5840/src

require github.com/as-family/zdb/proto v0.0.0-00010101000000-000000000000

replace github.com/as-family/zdb/proto => /home/ahmed/ws/zdb/out/build/gcc-14/go-proto
