# Convenience wrapper around CMake/CTest workflows for logman.
# Real build system is CMake — this just memorizes common invocations.

BUILD_DIR        ?= build
BUILD_TYPE       ?= Debug
JOBS             ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
GENERATOR        ?=
CMAKE            ?= cmake
CTEST            ?= ctest
LLVM_PROFDATA    ?= xcrun llvm-profdata
LLVM_COV         ?= xcrun llvm-cov

CMAKE_GEN_FLAG   := $(if $(GENERATOR),-G "$(GENERATOR)",)

.DEFAULT_GOAL := help

.PHONY: help
help:
	@echo "logman — make targets"
	@echo ""
	@echo "  make configure         Configure $(BUILD_DIR)/ ($(BUILD_TYPE))"
	@echo "  make build             Build everything in $(BUILD_DIR)/"
	@echo "  make test              Run ctest in $(BUILD_DIR)/"
	@echo "  make example           Run the logman_basic_usage example"
	@echo "  make examples          Build and run every logman_* example (fails on any non-zero exit)"
	@echo "  make all               configure + build + test"
	@echo ""
	@echo "  make sanitize          Configure+build+test in build-san/ with ASan+UBSan"
	@echo "  make tsan              Configure+build+test in build-tsan/ with ThreadSanitizer"
	@echo "  make tidy              Configure+build in build-tidy/ with clang-tidy"
	@echo "  make release           Configure+build in build-release/ (Release)"
	@echo "  make coverage          Configure+build+test in build-coverage/ with Clang coverage"
	@echo "  make docs              Configure+build Doxygen HTML in build-docs/"
	@echo ""
	@echo "  make format            Run clang-format -i over project sources"
	@echo "  make format-check      Verify formatting without writing"
	@echo ""
	@echo "  make ci                Run the full pre-push gate: format-check + tidy + test + sanitize + tsan + release"
	@echo ""
	@echo "  make clean             Remove $(BUILD_DIR)/"
	@echo "  make distclean         Remove all build-* directories"
	@echo ""
	@echo "Variables: BUILD_DIR=$(BUILD_DIR) BUILD_TYPE=$(BUILD_TYPE) JOBS=$(JOBS)"

.PHONY: configure
configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_GEN_FLAG) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

.PHONY: build
build: configure
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)

.PHONY: test
test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

.PHONY: example
example: build
	$(BUILD_DIR)/examples/logman_basic_usage

.PHONY: examples
examples: build
	@set -e; \
	failures=""; \
	for ex in $(BUILD_DIR)/examples/logman_*; do \
		if [ -x "$$ex" ] && [ ! -d "$$ex" ]; then \
			name=$$(basename "$$ex"); \
			echo "=== $$name ==="; \
			if ! "$$ex" >/dev/null; then \
				echo "FAIL: $$name (exit $$?)"; \
				failures="$$failures $$name"; \
			fi; \
		fi; \
	done; \
	if [ -n "$$failures" ]; then \
		echo "Examples that failed:$$failures"; \
		exit 1; \
	fi

.PHONY: all
all: test

.PHONY: ci
ci: format-check tidy test sanitize tsan release
	@echo ""
	@echo "ci: all checks passed"

.PHONY: sanitize
sanitize:
	$(CMAKE) -S . -B build-san $(CMAKE_GEN_FLAG) -DCMAKE_BUILD_TYPE=Debug -DLOGMAN_ENABLE_SANITIZERS=ON
	$(CMAKE) --build build-san -j $(JOBS)
	$(CTEST) --test-dir build-san --output-on-failure

.PHONY: tsan
tsan:
	$(CMAKE) -S . -B build-tsan $(CMAKE_GEN_FLAG) -DCMAKE_BUILD_TYPE=Debug -DLOGMAN_ENABLE_TSAN=ON
	$(CMAKE) --build build-tsan -j $(JOBS)
	$(CTEST) --test-dir build-tsan --output-on-failure

.PHONY: tidy
tidy:
	$(CMAKE) -S . -B build-tidy $(CMAKE_GEN_FLAG) -DCMAKE_BUILD_TYPE=Debug -DLOGMAN_ENABLE_CLANG_TIDY=ON
	$(CMAKE) --build build-tidy -j $(JOBS)

.PHONY: release
release:
	$(CMAKE) -S . -B build-release $(CMAKE_GEN_FLAG) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build build-release -j $(JOBS)
	$(CTEST) --test-dir build-release --output-on-failure

.PHONY: coverage
coverage:
	$(CMAKE) -S . -B build-coverage $(CMAKE_GEN_FLAG) \
		-DCMAKE_BUILD_TYPE=Debug -DLOGMAN_ENABLE_COVERAGE=ON
	$(CMAKE) --build build-coverage -j $(JOBS)
	rm -f build-coverage/*.profraw build-coverage/logman.profdata
	LLVM_PROFILE_FILE="$(CURDIR)/build-coverage/logman-%p.profraw" \
		$(CTEST) --test-dir build-coverage --output-on-failure
	$(LLVM_PROFDATA) merge -sparse build-coverage/*.profraw \
		-o build-coverage/logman.profdata
	$(LLVM_COV) report build-coverage/tests/logman_tests \
		-instr-profile=build-coverage/logman.profdata \
		-ignore-filename-regex='(_deps|tests)/'
	$(LLVM_COV) show build-coverage/tests/logman_tests \
		-instr-profile=build-coverage/logman.profdata \
		-ignore-filename-regex='(_deps|tests)/' \
		-format=html -output-dir=build-coverage/coverage-html \
		-show-line-counts-or-regions
	@echo "HTML report: build-coverage/coverage-html/index.html"

.PHONY: docs
docs:
	$(CMAKE) -S . -B build-docs $(CMAKE_GEN_FLAG) -DLOGMAN_BUILD_DOCS=ON
	$(CMAKE) --build build-docs --target logman_docs -j $(JOBS)
	@echo "HTML report: build-docs/docs/html/index.html"

FORMAT_FILES := $(shell find include tests examples -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cc' \) 2>/dev/null)

.PHONY: format
format:
	@if [ -z "$(FORMAT_FILES)" ]; then echo "no source files found"; else clang-format -i $(FORMAT_FILES); fi

.PHONY: format-check
format-check:
	@if [ -z "$(FORMAT_FILES)" ]; then echo "no source files found"; else clang-format --dry-run --Werror $(FORMAT_FILES); fi

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: distclean
distclean:
	rm -rf build build-san build-tsan build-tidy build-release build-coverage build-docs cmake-build-*
