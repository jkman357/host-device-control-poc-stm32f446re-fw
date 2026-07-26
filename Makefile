CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wshadow -Wundef -Wconversion -Wdouble-promotion -Wformat=2 -Werror -pedantic -IProtocol/Inc -IApp/Inc
.PHONY: host-test validator-test validate clean
host-test: build/protocol_roundtrip_test build/app_event_ordering_test build/app_command_ordering_test
	./build/protocol_roundtrip_test
	./build/app_event_ordering_test
	./build/app_command_ordering_test
build:
	mkdir -p build
build/protocol_roundtrip_test: build Tests/protocol_roundtrip_test.c Protocol/Src/protocol.c
	$(CC) $(CFLAGS) Tests/protocol_roundtrip_test.c Protocol/Src/protocol.c -o $@
build/app_event_ordering_test: build Tests/app_event_ordering_test.c App/Src/app_event.c
	$(CC) $(CFLAGS) -DAPP_EVENT_HOST_TEST Tests/app_event_ordering_test.c App/Src/app_event.c -o $@
build/app_command_ordering_test: build Tests/app_command_ordering_test.c App/Src/app.c App/Src/sine_generator.c Protocol/Src/protocol.c
	$(CC) $(CFLAGS) -ITransport/Inc -IPlatform/Inc Tests/app_command_ordering_test.c App/Src/app.c App/Src/sine_generator.c Protocol/Src/protocol.c -o $@
validator-test:
	python3 Tests/test_validate_project.py
validate:
	python3 Tools/validate_project.py
	$(MAKE) host-test
	$(MAKE) validator-test
	python3 Tools/protocol_command_sweep.py --self-test
	python3 Tools/serial_smoke_test.py --self-test
	python3 Tools/test_linker_layout.py
	bash Tools/build_with_clang.sh
clean:
	rm -rf build
