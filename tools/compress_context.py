#!/usr/bin/env python3
"""
LLMLingua context compressor — reduce token usage before Claude API calls.

Usage:
  # Compress a file
  python3 tools/compress_context.py --file README.md --rate 0.5

  # Compress stdin
  cat output/sample_path.json | python3 tools/compress_context.py --rate 0.3

  # Compress a string directly
  python3 tools/compress_context.py --text "long description..." --rate 0.5

  # Use as a library (see compress() function below)

Compression rates: 0.3 = keep 30% of tokens (aggressive), 0.5 = keep 50% (balanced), 0.7 = keep 70% (light)
LLMLingua-2 model: microsoft/llmlingua-2-bert-base-multilingual-cased-meetingbank (~400MB, CPU-friendly)
"""

import sys
import argparse
import json

_compressor = None


def get_compressor():
    """Lazy-load the model on first use (avoids slow startup when imported as library)."""
    global _compressor
    if _compressor is None:
        from llmlingua import PromptCompressor
        import warnings
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            _compressor = PromptCompressor(
                model_name="microsoft/llmlingua-2-bert-base-multilingual-cased-meetingbank",
                use_llmlingua2=True,
                device_map="cpu",
            )
    return _compressor


def compress(text: str, rate: float = 0.5, force_tokens: list = None) -> dict:
    """
    Compress text using LLMLingua-2.

    Args:
        text: The text to compress (e.g. a long system prompt, doc, or tool output).
        rate: Fraction of tokens to keep. 0.5 = ~50% reduction.
        force_tokens: Tokens that must not be dropped (default: newlines, punctuation).

    Returns:
        dict with keys: compressed_prompt (str), ratio (str), origin_tokens (int), compressed_tokens (int)

    Example — compress a long OSM description before sending to Claude:
        result = compress(long_osm_readme, rate=0.4)
        print(result["compressed_prompt"])   # feed this to Claude instead
        print(result["ratio"])               # e.g. "2.5x"
    """
    if force_tokens is None:
        force_tokens = ["\n", ".", "?", "!", ":"]

    compressor = get_compressor()
    result = compressor.compress_prompt(
        [text],
        rate=rate,
        force_tokens=force_tokens,
    )
    return result


def _estimate_tokens(text: str) -> int:
    """Rough token estimate: ~4 chars per token."""
    return max(1, len(text) // 4)


def main():
    parser = argparse.ArgumentParser(description="Compress text with LLMLingua-2 to reduce Claude token usage")
    parser.add_argument("--file", help="Path to file to compress")
    parser.add_argument("--text", help="Text string to compress")
    parser.add_argument("--rate", type=float, default=0.5,
                        help="Fraction of tokens to keep (default: 0.5 = 50%% kept, ~2x compression)")
    parser.add_argument("--json-output", action="store_true",
                        help="Output JSON with stats instead of plain compressed text")
    args = parser.parse_args()

    # Read input
    if args.file:
        with open(args.file, "r") as f:
            text = f.read()
    elif args.text:
        text = args.text
    elif not sys.stdin.isatty():
        text = sys.stdin.read()
    else:
        parser.print_help()
        sys.exit(1)

    orig_tokens = _estimate_tokens(text)
    print(f"Compressing {len(text):,} chars (~{orig_tokens:,} tokens) at rate={args.rate}...", file=sys.stderr)

    result = compress(text, rate=args.rate)

    compressed_tokens = _estimate_tokens(result["compressed_prompt"])
    saved = orig_tokens - compressed_tokens

    if args.json_output:
        print(json.dumps({
            "compressed_prompt": result["compressed_prompt"],
            "ratio": result.get("ratio"),
            "origin_tokens": orig_tokens,
            "compressed_tokens": compressed_tokens,
            "tokens_saved": saved,
            "reduction_pct": round(saved / orig_tokens * 100, 1),
        }, indent=2))
    else:
        print(result["compressed_prompt"])
        print(f"\n--- Stats: {orig_tokens} → {compressed_tokens} tokens saved "
              f"({round(saved/orig_tokens*100)}% reduction, {result.get('ratio','?')} compression) ---",
              file=sys.stderr)


if __name__ == "__main__":
    main()
