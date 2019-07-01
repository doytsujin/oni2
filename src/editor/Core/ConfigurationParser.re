/*
 * ConfigurationParser.re
 *
 * Resilient parsing for Configuration
 */
open ConfigurationValues;
open LineNumber;

let parseBool = json =>
  switch (json) {
  | `Bool(v) => v
  | _ => false
  };

let parseInt = json =>
  switch (json) {
  | `Int(v) => v
  | _ => 0
  };

let parseStringList = json => {
  switch (json) {
  | `List(items) =>
    List.fold_left(
      (accum, item) =>
        switch (item) {
        | `String(v) => [v, ...accum]
        | _ => accum
        },
      [],
      items,
    )
  | _ => []
  };
};

let parseLineNumberSetting = json =>
  switch (json) {
  | `String(v) =>
    switch (v) {
    | "on" => On
    | "off" => Off
    | "relative" => Relative
    | _ => On
    }
  | _ => On
  };

let parseRenderWhitespace = json =>
  switch (json) {
  | `String(v) =>
    switch (v) {
    | "all" => All
    | "boundary" => Boundary
    | "none" => None
    | _ => All
    }
  | _ => All
  };

let parseString = json =>
  switch (json) {
  | `String(v) => v
  | _ => ""
  };

type parseFunction =
  (ConfigurationValues.t, Yojson.Safe.json) => ConfigurationValues.t;

type configurationTuple = (string, parseFunction);

let configurationParsers: list(configurationTuple) = [
  (
    "editor.lineNumbers",
    (s, v) => {...s, editorLineNumbers: parseLineNumberSetting(v)},
  ),
  (
    "editor.minimap.enabled",
    (s, v) => {...s, editorMinimapEnabled: parseBool(v)},
  ),
  (
    "editor.minimap.showSlider",
    (s, v) => {...s, editorMinimapShowSlider: parseBool(v)},
  ),
  (
    "editor.minimap.maxColumn",
    (s, v) => {...s, editorMinimapMaxColumn: parseInt(v)},
  ),
  (
    "editor.minimap.showSlider",
    (s, v) => {...s, editorMinimapShowSlider: parseBool(v)},
  ),
  (
    "editor.detectIndentation",
    (s, v) => {...s, editorDetectIndentation: parseBool(v)},
  ),
  (
    "editor.insertSpaces",
    (s, v) => {...s, editorInsertSpaces: parseBool(v)},
  ),
  ("editor.indentSize", (s, v) => {...s, editorIndentSize: parseInt(v)}),
  ("editor.tabSize", (s, v) => {...s, editorTabSize: parseInt(v)}),
  (
    "editor.highlightActiveIndentGuide",
    (s, v) => {...s, editorHighlightActiveIndentGuide: parseBool(v)},
  ),
  (
    "editor.renderIndentGuides",
    (s, v) => {...s, editorRenderIndentGuides: parseBool(v)},
  ),
  (
    "editor.renderWhitespace",
    (s, v) => {...s, editorRenderWhitespace: parseRenderWhitespace(v)},
  ),
  ("files.exclude", (s, v) => {...s, filesExclude: parseStringList(v)}),
  (
    "workbench.iconTheme",
    (s, v) => {...s, workbenchIconTheme: parseString(v)},
  ),
  (
    "workbench.editor.showTabs",
    (s, v) => {...s, workbenchEditorShowTabs: parseBool(v)},
  ),
];

let keyToParser: Hashtbl.t(string, parseFunction) =
  List.fold_left(
    (prev, cur) => {
      let (key, parser) = cur;
      Hashtbl.add(prev, key, parser);
      prev;
    },
    Hashtbl.create(100),
    configurationParsers,
  );

type parseResult = {
  nestedConfigurations: list((string, Yojson.Safe.json)),
  configurationValues: ConfigurationValues.t,
};

let isFiletype = (str: string) => {
  let str = String.trim(str);

  let len = String.length(str);

  if (len >= 3) {
    str.[0] == '[' && str.[len - 1] == ']';
  } else {
    false;
  };
};

let getFiletype = (str: string) => {
  let str = String.trim(str);
  let len = String.length(str);

  String.sub(str, 1, len - 2);
};

let parse: list((string, Yojson.Safe.json)) => parseResult =
  items => {
    List.fold_left(
      (prev, cur) => {
        let (key, json) = cur;
        let {nestedConfigurations, configurationValues} = prev;

        isFiletype(key)
          ? {
            let nestedConfigurations = [
              (getFiletype(key), json),
              ...nestedConfigurations,
            ];
            {nestedConfigurations, configurationValues};
          }
          : (
            switch (Hashtbl.find_opt(keyToParser, key)) {
            | Some(v) => {
                nestedConfigurations,
                configurationValues: v(configurationValues, json),
              }
            | None => prev
            }
          );
      },
      {
        nestedConfigurations: [],
        configurationValues: ConfigurationValues.default,
      },
      items,
    );
  };

let parseNested = (json: Yojson.Safe.json, default: ConfigurationValues.t) => {
  switch (json) {
  | `Assoc(items) =>
    List.fold_left(
      (prev, cur) => {
        let (key, json) = cur;

        isFiletype(key)
          ? prev
          : (
            switch (Hashtbl.find_opt(keyToParser, key)) {
            | Some(v) => v(prev, json)
            | None => prev
            }
          );
      },
      default,
      items,
    )
  | _ => default
  };
};

let ofJson = json => {
  switch (json) {
  | `Assoc(items) =>
    let {configurationValues, nestedConfigurations} = parse(items);

    let perFiletype =
      List.fold_left(
        (prev, cur) => {
          let (key, json) = cur;
          StringMap.add(key, parseNested(json, configurationValues), prev);
        },
        StringMap.empty,
        nestedConfigurations,
      );

    let configuration =
      Configuration.{default: configurationValues, perFiletype};
    Ok(configuration);
  | _ => Error("Incorrect JSON format for configuration")
  };
};

let ofString = str => {
  switch (str |> Yojson.Safe.from_string |> ofJson) {
  | v => v
  | exception (Yojson.Json_error(msg)) => Error(msg)
  };
};

let ofFile = filePath =>
  switch (Yojson.Safe.from_file(filePath) |> ofJson) {
  | v => v
  | exception (Yojson.Json_error(msg)) => Error(msg)
  };
