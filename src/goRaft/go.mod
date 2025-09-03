module goRaft

go 1.23

toolchain go1.24.1

require 6.5840 v0.0.0-00010101000000-000000000000

require (
	github.com/anishathalye/porcupine v1.0.3 // indirect
	google.golang.org/protobuf v1.36.8 // indirect
)

replace 6.5840 => ../../6.5840/src

require proto v0.0.0-00010101000000-000000000000

replace proto => ../../out/build/gcc-14/go-proto
