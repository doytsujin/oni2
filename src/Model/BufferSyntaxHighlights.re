/*
 * BufferSyntaxHighlights.re
 *
 * State kept for per-buffer syntax highlighting
 */
open Oni_Core;
open Oni_Core.Utility;
open Oni_Syntax;

open Revery;

module BufferMap = IntMap;

module LineMap = IntMap;

type t = BufferMap.t(LineMap.t(list(ColorizedToken.t)));

let empty = BufferMap.empty;

let noTokens = [];

module ClientLog = (
  val Oni_Core.Log.withNamespace("BufferSyntaxHighlights - TESTING")
);

let getTokens = (bufferId: int, line: int, highlights: t) => {
  highlights
  |> BufferMap.find_opt(bufferId)
  |> Option.bind(LineMap.find_opt(line))
  |> Option.value(~default=noTokens);
};

let setTokensForLine =
    (bufferId: int, line: int, tokens: list(ColorizedToken.t), highlights: t) => {
  let updateLineMap = (lineMap: LineMap.t(list(ColorizedToken.t))) => {
    LineMap.update(line, _ => Some(tokens), lineMap);
  };

  BufferMap.update(
    bufferId,
    fun
    | None => Some(updateLineMap(LineMap.empty))
    | Some(v) => Some(updateLineMap(v)),
    highlights,
  );
};

let setTokens = (tokenUpdates: list(Protocol.TokenUpdate.t), highlights: t) => {
  Protocol.TokenUpdate.(
    tokenUpdates
    |> List.fold_left(
         (acc, curr) => {
           setTokensForLine(curr.bufferId, curr.line, curr.tokenColors, acc)
         },
         highlights,
       )
  );
};

// When there is a buffer update, shift the lines to match
let handleUpdate = (bufferUpdate: BufferUpdate.t, highlights: t) => {
  BufferMap.update(
    bufferUpdate.id,
    fun
    | None => None
    | Some(lineMap) =>
      Some(
        LineMap.shift(
          ~default=v => v,
          ~startPos=bufferUpdate.startLine |> Index.toInt0,
          ~endPos=bufferUpdate.endLine |> Index.toInt0,
          ~delta=Array.length(bufferUpdate.lines),
          lineMap,
        ),
      ),
    highlights,
  );
};