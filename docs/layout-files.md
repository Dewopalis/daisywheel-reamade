# Character layout files

Daisywheel imports and exports UTF-8 plain-text layout files. They use a small,
labeled format that can be edited in Notepad, Kate, Gedit, or another text
editor.

Use **Settings → Controller → Export layout file** to create a complete starter
file. Edit the characters, then choose **Import layout file**. Up to eight
custom layouts are saved in Daisywheel settings, so their source files can be
moved or deleted after import. Importing a layout with the same name replaces
the saved layout with that name. Select a custom layout and choose **Remove
selected custom layout** to delete it from Daisywheel settings.

## Format

The first line identifies the file format version. Keep `DAISYWHEEL LAYOUT V1`
unchanged when editing; the `V1` lets Daisywheel distinguish this format from
future versions. The name appears in Daisywheel's
layout selector. Each section has one line for every petal, with four
space-separated characters in **A, B, X, Y** order.

```text
DAISYWHEEL LAYOUT V1
Name = My layout

[letters.lower]
North = a b c d
Northeast = e f g h
East = i j k l
Southeast = m n o p
South = q r s t
Southwest = u v w x
West = y z . ,
Northwest = ! ? ' -

[letters.upper]
North = A B C D
...
```

The abbreviated example shows the format. Export a layout first to get a
complete file with all required rows and sections:

- `[letters.lower]` and `[letters.upper]`
- `[symbols]`
- `[extended.lower]` and `[extended.upper]`

Petals are listed clockwise from North. Names are case-insensitive. A line
starting with `#` is a comment. Each slot must be exactly one Unicode code
point, so use precomposed accented letters such as `ê`. Keep four values on
each petal line.

Import accepts files up to 64 KB and checks the entire file before saving it.
Select **Custom: name** in the Character layout setting to use it; RB then cycles
letters, symbols, and the extended page. Choosing a built-in layout restores
the built-in characters.
