PROJECT_NAME = messenger_service
NPROCS ?= $(shell nproc 2>/dev/null || echo 4)
CLANG_FORMAT ?= clang-format
DOCKER_IMAGE ?= ghcr.io/userver-framework/ubuntu-24.04-userver:latest
DOCKER_ARGS = $(shell /bin/test -t 0 && /bin/echo -it || echo)
PRESETS ?= debug release

.PHONY: all
all: test-debug test-release

.PHONY: $(addprefix cmake-, $(PRESETS))
$(addprefix cmake-, $(PRESETS)): cmake-%:
	cmake --preset $*

$(addsuffix /CMakeCache.txt, $(addprefix build-, $(PRESETS))): build-%/CMakeCache.txt:
	$(MAKE) cmake-$*

.PHONY: $(addprefix build-, $(PRESETS))
$(addprefix build-, $(PRESETS)): build-%: build-%/CMakeCache.txt
	cmake --build build-$* -j $(NPROCS) --target $(PROJECT_NAME)

.PHONY: $(addprefix test-, $(PRESETS))
$(addprefix test-, $(PRESETS)): test-%: build-%/CMakeCache.txt
	cmake --build build-$* -j $(NPROCS)
	cd build-$* && ((test -t 1 && GTEST_COLOR=1 PYTEST_ADDOPTS="--color=yes" ctest -V) || ctest -V)

.PHONY: $(addprefix start-, $(PRESETS))
$(addprefix start-, $(PRESETS)): start-%:
	cmake --build build-$* -v --target start-$(PROJECT_NAME)

.PHONY: dist-clean
dist-clean:
	rm -rf build* tests/__pycache__ tests/.pytest_cache .ccache

.PHONY: format
format:
	find src -name '*pp' -type f | xargs $(CLANG_FORMAT) -i
	find tests -name '*.py' -type f | xargs autopep8 -i 2>/dev/null || true

.PHONY: $(addprefix docker-, $(PRESETS)) docker-test-debug docker-build-debug docker-start-debug
$(addprefix docker-, $(PRESETS)) docker-test-debug docker-build-debug docker-start-debug: docker-%:
	docker run $(DOCKER_ARGS) --network=host -v $$PWD:$$PWD -w $$PWD $(DOCKER_IMAGE) \
		env CCACHE_DIR=$$PWD/.ccache HOME=$$HOME $$PWD/run_as_user.sh $$(id -u) $$(id -g) make $*
