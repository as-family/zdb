package main

import (
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/anishathalye/porcupine"
)

// Operation represents a single operation for porcupine
type Operation struct {
	ClientID   int    `json:"client_id"`
	Input      Input  `json:"input"`
	Output     Output `json:"output"`
	CallTime   int64  `json:"call_time"`
	ReturnTime int64  `json:"return_time"`
}

// Input represents the input to an operation
type Input struct {
	Op      int    `json:"op"` // 0 = Get, 1 = Put
	Key     string `json:"key"`
	Value   string `json:"value"`
	Version int64  `json:"version"`
}

// Output represents the output of an operation
type Output struct {
	Value   string `json:"value"`
	Version int64  `json:"version"`
	Error   string `json:"error"`
}

// KVState represents the state of the key-value store
type KVState struct {
	Data map[string]string `json:"data"`
}

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintf(os.Stderr, "Usage: %s <operations.json>\n", os.Args[0])
		os.Exit(1)
	}

	filename := os.Args[1]
	data, err := os.ReadFile(filename)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file: %v\n", err)
		os.Exit(1)
	}

	var operations []Operation
	if err := json.Unmarshal(data, &operations); err != nil {
		fmt.Fprintf(os.Stderr, "Error parsing JSON: %v\n", err)
		os.Exit(1)
	}

	// Convert to porcupine operations
	var porcupineOps []porcupine.Operation
	for _, op := range operations {
		porcupineOps = append(porcupineOps, porcupine.Operation{
			ClientId: op.ClientID,
			Input:    op.Input,
			Call:     op.CallTime,
			Output:   op.Output,
			Return:   op.ReturnTime,
		})
	}

	// Define KV store model
	kvModel := porcupine.Model{
		Init: func() interface{} {
			return KVState{Data: make(map[string]string)}
		},
		Step: func(state interface{}, input interface{}, output interface{}) (bool, interface{}) {
			st := state.(KVState)
			inp := input.(Input)
			out := output.(Output)

			switch inp.Op {
			case 0: // Get
				if out.Error == "OK" {
					if val, exists := st.Data[inp.Key]; exists {
						if val == out.Value {
							return true, st
						}
					}
					return false, st
				} else if out.Error == "ErrNoKey" {
					_, exists := st.Data[inp.Key]
					return !exists, st
				}
				return false, st

			case 1: // Put
				if out.Error == "OK" {
					newState := KVState{Data: make(map[string]string)}
					for k, v := range st.Data {
						newState.Data[k] = v
					}
					newState.Data[inp.Key] = inp.Value
					return true, newState
				}
				return true, st // Put can fail but state doesn't change

			default:
				return false, st
			}
		},
	}

	// Check linearizability with 5 second timeout
	result := porcupine.CheckOperationsTimeout(kvModel, porcupineOps, 5*time.Second)

	if result == porcupine.Ok {
		fmt.Println("LINEARIZABLE")
		os.Exit(0)
	} else {
		fmt.Printf("NOT_LINEARIZABLE: %v\n", result)
		os.Exit(1)
	}
}
