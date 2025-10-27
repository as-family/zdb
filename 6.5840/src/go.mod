module 6.5840

go 1.24.1

require github.com/anishathalye/porcupine v1.0.3

require github.com/as-family/zdb v0.0.0-00010101000000-000000000000

require google.golang.org/protobuf v1.36.8 // indirect

replace github.com/as-family/zdb => ../../src/goRaft

require github.com/as-family/zdb/proto v0.0.0-00010101000000-000000000000 // indirect

replace github.com/as-family/zdb/proto => ../../out/build/sys-gcc/go-proto
