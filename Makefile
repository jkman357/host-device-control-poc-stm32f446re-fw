CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wshadow -Wundef -Wconversion -Wdouble-promotion -Wformat=2 -Werror -pedantic -IProtocol/Inc -IApp/Inc -ITransport/Inc -IPlatform/Inc
APP_TEST_SOURCES := App/Src/app.c App/Src/waveform_generator.c Protocol/Src/protocol.c
.PHONY: host-test validator-test validate clean
host-test: build/protocol_roundtrip_test build/app_event_ordering_test build/app_command_ordering_test build/waveform_generator_test build/app_waveform_rotation_test build/serial_baud_profile_test build/app_baud_policy_9600_test build/app_baud_policy_19200_test build/app_baud_policy_460800_test build/app_baud_policy_921600_test
	./build/protocol_roundtrip_test
	./build/app_event_ordering_test
	./build/app_command_ordering_test
	./build/waveform_generator_test
	./build/app_waveform_rotation_test
	./build/serial_baud_profile_test
	./build/app_baud_policy_9600_test
	./build/app_baud_policy_19200_test
	./build/app_baud_policy_460800_test
	./build/app_baud_policy_921600_test
build:
	mkdir -p build
build/protocol_roundtrip_test: build Tests/protocol_roundtrip_test.c Protocol/Src/protocol.c
	$(CC) $(CFLAGS) Tests/protocol_roundtrip_test.c Protocol/Src/protocol.c -o $@
build/app_event_ordering_test: build Tests/app_event_ordering_test.c App/Src/app_event.c
	$(CC) $(CFLAGS) -DAPP_EVENT_HOST_TEST Tests/app_event_ordering_test.c App/Src/app_event.c -o $@
build/app_command_ordering_test: build Tests/app_command_ordering_test.c $(APP_TEST_SOURCES)
	$(CC) $(CFLAGS) Tests/app_command_ordering_test.c $(APP_TEST_SOURCES) -o $@
build/waveform_generator_test: build Tests/waveform_generator_test.c App/Src/waveform_generator.c
	$(CC) $(CFLAGS) Tests/waveform_generator_test.c App/Src/waveform_generator.c -o $@
build/app_waveform_rotation_test: build Tests/app_waveform_rotation_test.c $(APP_TEST_SOURCES)
	$(CC) $(CFLAGS) Tests/app_waveform_rotation_test.c $(APP_TEST_SOURCES) -o $@
build/serial_baud_profile_test: build Tests/serial_baud_profile_test.c
	$(CC) $(CFLAGS) Tests/serial_baud_profile_test.c -o $@
build/app_baud_policy_9600_test: build Tests/app_baud_policy_test.c $(APP_TEST_SOURCES)
	$(CC) $(CFLAGS) -DSERIAL_TRANSPORT_BAUD_RATE=9600u Tests/app_baud_policy_test.c $(APP_TEST_SOURCES) -o $@
build/app_baud_policy_19200_test: build Tests/app_baud_policy_test.c $(APP_TEST_SOURCES)
	$(CC) $(CFLAGS) -DSERIAL_TRANSPORT_BAUD_RATE=19200u Tests/app_baud_policy_test.c $(APP_TEST_SOURCES) -o $@
build/app_baud_policy_460800_test: build Tests/app_baud_policy_test.c $(APP_TEST_SOURCES)
	$(CC) $(CFLAGS) -DSERIAL_TRANSPORT_BAUD_RATE=460800u Tests/app_baud_policy_test.c $(APP_TEST_SOURCES) -o $@
build/app_baud_policy_921600_test: build Tests/app_baud_policy_test.c $(APP_TEST_SOURCES)
	$(CC) $(CFLAGS) -DSERIAL_TRANSPORT_BAUD_RATE=921600u Tests/app_baud_policy_test.c $(APP_TEST_SOURCES) -o $@
validator-test:
	python3 Tests/test_validate_project.py
validate:
	python3 Tools/validate_project.py
	$(MAKE) host-test
	$(MAKE) validator-test
	python3 Tools/protocol_command_sweep.py --self-test
	python3 Tools/serial_smoke_test.py --self-test
	python3 Tools/test_linker_layout.py
	bash Tools/build_all_baud_profiles.sh
clean:
	rm -rf build
