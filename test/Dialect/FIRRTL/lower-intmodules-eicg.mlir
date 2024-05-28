// RUN: circt-opt --pass-pipeline='builtin.module(firrtl.circuit(firrtl-lower-intmodules))' --split-input-file %s | FileCheck %s --check-prefixes=CHECK,CHECK-NOEICG
// RUN: circt-opt --pass-pipeline='builtin.module(firrtl.circuit(firrtl-lower-intmodules{fixup-eicg-wrapper}))' --split-input-file %s  --verify-diagnostics | FileCheck %s --check-prefixes=CHECK,CHECK-EICG

// CHECK-LABEL: "FixupEICGWrapper"
firrtl.circuit "FixupEICGWrapper" {
  // CHECK-NOEICG: LegacyClockGate
  // CHECK-EICG-NOT: LegacyClockGate
  // expected-warning @below {{Annotation firrtl.transforms.DedupGroupAnnotation on EICG_wrapper is dropped}}
  firrtl.extmodule @LegacyClockGate(in in: !firrtl.clock, in test_en: !firrtl.uint<1>, in en: !firrtl.uint<1>, out out: !firrtl.clock) attributes {
    defname = "EICG_wrapper",
    annotations = [{class = "firrtl.transforms.DedupGroupAnnotation", group = "foo"}]}

  // CHECK: FixupEICGWrapper
  firrtl.module @FixupEICGWrapper(in %clock: !firrtl.clock, in %en: !firrtl.uint<1>) {
    // CHECK-NOEICG: firrtl.instance
    // CHECK-EICG-NOT: firrtl.instance
    // CHECK-EICG: firrtl.int.generic "circt_clock_gate"
    %ckg_in, %ckg_test_en, %ckg_en, %ckg_out = firrtl.instance ckg @LegacyClockGate(in in: !firrtl.clock, in test_en: !firrtl.uint<1>, in en: !firrtl.uint<1>, out out: !firrtl.clock)
    %ckg_in_read, %ckg_in_write = firrtl.deduplex %ckg_in : !firrtl.clock
    firrtl.strictconnect %ckg_in_write, %clock : !firrtl.clock
    %ckg_test_en_read, %ckg_test_en_write = firrtl.deduplex %ckg_test_en : !firrtl.uint<1>
    firrtl.strictconnect %ckg_test_en_write, %en : !firrtl.uint<1>
    %ckg_en_read, %ckg_en_write = firrtl.deduplex %ckg_en : !firrtl.uint<1>
    firrtl.strictconnect %ckg_en_write, %en : !firrtl.uint<1>
  }
}

// -----

// CHECK-LABEL: "FixupEICGWrapper2"
firrtl.circuit "FixupEICGWrapper2" {
  // CHECK-NOEICG: LegacyClockGateNoTestEn
  // CHECK-EICG-NOT: LegacyClockGateNoTestEn
  firrtl.extmodule @LegacyClockGateNoTestEn(in in: !firrtl.clock, in en: !firrtl.uint<1>, out out: !firrtl.clock) attributes {defname = "EICG_wrapper"}

  // CHECK: FixupEICGWrapper2
  firrtl.module @FixupEICGWrapper2(in %clock: !firrtl.clock, in %en: !firrtl.uint<1>) {
    // CHECK-NOEICG: firrtl.instance
    // CHECK-EICG-NOT: firrtl.instance
    // CHECK-EICG: firrtl.int.generic "circt_clock_gate"
    %ckg_in, %ckg_en, %ckg_out = firrtl.instance ckg @LegacyClockGateNoTestEn(in in: !firrtl.clock, in en: !firrtl.uint<1>, out out: !firrtl.clock)
    %ckg_in_read, %ckg_in_write = firrtl.deduplex %ckg_in : !firrtl.clock
    firrtl.strictconnect %ckg_in_write, %clock : !firrtl.clock
    %ckg_en_read, %ckg_en_write = firrtl.deduplex %ckg_en : !firrtl.uint<1>
    firrtl.strictconnect %ckg_en_write, %en : !firrtl.uint<1>
  }
}

