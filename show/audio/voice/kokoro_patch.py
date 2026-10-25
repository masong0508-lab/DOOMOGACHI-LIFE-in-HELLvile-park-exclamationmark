"""Patch the Kokoro-82M v1.0 ONNX graph so it can SING.

Kokoro predicts, per phoneme token, a duration (in 25 ms frames) and, per 12.5 ms
frame, an F0 curve; the decoder (hn-NSF source + iSTFTNet) then renders audio that
follows that F0. This script writes two models next to the original:

  kokoro_analysis.onnx  original inputs; extra outputs dur_pred [1,T] int64,
                        f0_pred [1,2F] float, n_pred [1,2F] float (to read what the
                        model would do for normal speech)
  kokoro_sing.onnx      extra INPUTS dur_in [1,T] int64, f0_in [1,2F] and n_in [1,2F]
                        float that replace the predicted durations, F0 and energy:
                        we place every phoneme on the beat, give the voice a pitch
                        and hold the vowel's energy through long notes. Output
                        n_pred is the model's own energy for the given durations.

The graph surgery only rewires tensor names; weights are untouched.
"""
from __future__ import annotations

import os
import sys

import onnx
from onnx import TensorProto, helper

HERE = os.path.dirname(os.path.abspath(__file__))
# show/audio vendoring: KOKORO_MODELS (output dir) / KOKORO_SRC (fp32 model) override
MODELS = os.environ.get("KOKORO_MODELS") or os.path.abspath(os.path.join(HERE, "..", "work", "models"))
SRC = os.environ.get("KOKORO_SRC") or os.path.abspath(
    os.path.join(HERE, "..", "..", "final_dl", "npm_kokoro_fp32", "kokoro-fp32.onnx"))

DUR = "/encoder/Cast_output_0"        # rounded, clipped durations cast to int64
F0 = "/encoder/Squeeze_output_0"      # F0_proj output squeezed: [1, 2*frames]
NN = "/encoder/Squeeze_1_output_0"    # N_proj (energy) output


def consumers(g, name):
    return [n for n in g.node if name in n.input]


def main():
    os.makedirs(MODELS, exist_ok=True)
    m = onnx.load(SRC)
    g = m.graph
    for nm in (DUR, F0, NN):
        assert consumers(g, nm), nm
    # analysis model: expose predictions
    a = onnx.ModelProto()
    a.CopyFrom(m)
    ga = a.graph
    ga.output.append(helper.make_tensor_value_info("dur_pred", TensorProto.INT64, [1, "T"]))
    ga.output.append(helper.make_tensor_value_info("f0_pred", TensorProto.FLOAT, [1, "F2"]))
    ga.output.append(helper.make_tensor_value_info("n_pred", TensorProto.FLOAT, [1, "F2"]))
    ga.node.append(helper.make_node("Identity", [DUR], ["dur_pred"]))
    ga.node.append(helper.make_node("Identity", [F0], ["f0_pred"]))
    ga.node.append(helper.make_node("Identity", [NN], ["n_pred"]))
    onnx.save(a, os.path.join(MODELS, "kokoro_analysis.onnx"))
    # singing model: override durations and F0
    s = onnx.ModelProto()
    s.CopyFrom(m)
    gs = s.graph
    gs.input.append(helper.make_tensor_value_info("dur_in", TensorProto.INT64, [1, "T"]))
    gs.input.append(helper.make_tensor_value_info("f0_in", TensorProto.FLOAT, [1, "F2"]))
    gs.input.append(helper.make_tensor_value_info("n_in", TensorProto.FLOAT, [1, "F2"]))
    for node in gs.node:
        for i, x in enumerate(node.input):
            if x == DUR:
                node.input[i] = "dur_in"
            elif x == F0:
                node.input[i] = "f0_in"
            elif x == NN:
                node.input[i] = "n_in"
    gs.output.append(helper.make_tensor_value_info("n_pred", TensorProto.FLOAT, [1, "F2"]))
    gs.node.append(helper.make_node("Identity", [NN], ["n_pred"]))
    onnx.save(s, os.path.join(MODELS, "kokoro_sing.onnx"))
    print("wrote", os.listdir(MODELS))


if __name__ == "__main__":
    sys.exit(main())
