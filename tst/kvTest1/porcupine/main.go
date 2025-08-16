package main

import (
	"encoding/json"
	"fmt"
	"os"
	"sort"
	"strconv"
	"time"

	"github.com/anishathalye/porcupine"
)

type Status uint8

const (
	OK Status = iota
	InvalidArg
	ServiceTemporarilyUnavailable
	AllServicesUnavailable
	VersionMismatch
	Maybe
	KeyNotFound
	TimeOut
	Unknown
)

type Operation struct {
	ClientID int    `json:"clientId"`
	Input    Input  `json:"input"`
	Output   Output `json:"output"`
	Start    int64  `json:"start"`
	End      int64  `json:"end"`
}

type Input struct {
	Op      int    `json:"op"`
	Key     string `json:"key"`
	Value   string `json:"value"`
	Version int64  `json:"version"`
}

type Output struct {
	Value   string `json:"value"`
	Version int64  `json:"version"`
	Error   Status `json:"error"`
}

type State struct {
	Value   string `json:"value"`
	Version int64  `json:"version"`
}

var KvModel = porcupine.Model{
	Partition: func(history []porcupine.Operation) [][]porcupine.Operation {
		m := make(map[string][]porcupine.Operation)
		for _, v := range history {
			key := v.Input.(Input).Key
			m[key] = append(m[key], v)
		}
		keys := make([]string, 0, len(m))
		for k := range m {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		ret := make([][]porcupine.Operation, 0, len(keys))
		for _, k := range keys {
			ret = append(ret, m[k])
		}
		return ret
	},
	Init: func() interface{} {
		// note: we are modeling a single key's value here;
		// we're partitioning by key, so this is okay
		return State{"", 0}
	},
	Step: func(state, input, output interface{}) (bool, interface{}) {
		inp := input.(Input)
		out := output.(Output)
		st := state.(State)
		switch inp.Op {
		case 2:
			// get
			return out.Value == st.Value, state
		case 1:
			// set
			if st.Version == inp.Version {
				return out.Error == OK || out.Error == Maybe, State{inp.Value, st.Version + 1}
			} else {
				return out.Error == VersionMismatch || out.Error == Maybe, st
			}
		default:
			return false, "<invalid>"
		}
	},
	DescribeOperation: func(input, output interface{}) string {
		inp := input.(Input)
		out := output.(Output)
		switch inp.Op {
		case 2:
			return fmt.Sprintf("get('%s') -> ('%s', '%d', '%d')", inp.Key, out.Value, out.Version, out.Error)
		case 1:
			return fmt.Sprintf("set('%s', '%s', '%d') -> ('%d')", inp.Key, inp.Value, inp.Version, out.Error)
		default:
			return "<invalid>"
		}
	},
}

func main() {
	if len(os.Args) != 4 {
		fmt.Fprintf(os.Stderr, "Usage: %s <operations.json> <timeout> <visualize/no>\n", os.Args[0])
		os.Exit(1)
	}

	filename := os.Args[1]
	data, err := os.ReadFile(filename)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file: %v\n", err)
		os.Exit(1)
	}

	timeout, err := strconv.Atoi(os.Args[2])
	if err != nil {
		fmt.Fprintf(os.Stderr, "Invalid timeout value: %v\n", err)
		os.Exit(1)
	}

	var operations []Operation
	if err := json.Unmarshal(data, &operations); err != nil {
		fmt.Fprintf(os.Stderr, "Error parsing JSON: %v\n", err)
		os.Exit(1)
	}
	var porcupineOps []porcupine.Operation
	for _, op := range operations {
		porcupineOps = append(porcupineOps, porcupine.Operation{
			ClientId: op.ClientID,
			Input:    op.Input,
			Call:     op.Start,
			Output:   op.Output,
			Return:   op.End,
		})
	}
	res, info := porcupine.CheckOperationsVerbose(KvModel, porcupineOps, time.Duration(timeout)*time.Second)

	visualize := os.Args[3] == "visualize"

	if visualize {
		var file *os.File
		var err error

		file, err = os.CreateTemp("", "porcupine-*.html")
		if err != nil {
			fmt.Printf("info: failed to create temporary visualization file (%v)\n", err)
		} else {
			err = porcupine.Visualize(KvModel, info, file)
			if err != nil {
				fmt.Printf("info: failed to write history visualization to %s\n", file.Name())
			} else {
				fmt.Printf("info: wrote history visualization to %s\n", file.Name())
			}
		}
	}
	fmt.Println(res)
	if res == porcupine.Ok {
		os.Exit(0)
	} else {
		os.Exit(1)
	}
}
