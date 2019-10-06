    /* root style, must be complete */
    STYLE_DEF(QE_STYLE_DEFAULT, "default",
              QERGB(0xff, 0xff, 0xff), QERGB(0x00, 0x00, 0x00),
              QE_FONT_FAMILY_FIXED, 10, 0)

    /* system styles */
    STYLE_DEF(QE_STYLE_MODE_LINE, "mode-line",
              QERGB(0xff, 0xff, 0xff), QERGB(0xf, 0xf, 0xf),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_ACTIVE_MODE_LINE, "mode-line",
              QERGB(0xff, 0xff, 0xff), QERGB(0x00, 0x00, 0xff),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_WINDOW_BORDER, "window-border",
              QERGB(0x00, 0x00, 0x00), QERGB(0xe0, 0xe0, 0xe0),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_MINIBUF, "minibuf",
              QERGB(0x00, 0xd8, 0xd8), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_STATUS, "status",
              QERGB(0xd8, 0x00, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_GUTTER, "gutter",
              QERGB(0xd8, 0xd8, 0xd8), QERGB(0x3f, 0x3f, 0x3f),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_COLUMN_INDICATOR, "column-indicator",
              QERGB(0xd8, 0xd8, 0xd8), QERGB(0x00, 0x00, 0x00),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_LINUM, "linum",
              QERGB(0x3f, 0x3f, 0x3f), QERGB(0x00, 0x00, 0x00),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_CURRENT_LINUM, "current-linum",
              QERGB(0, 0xff, 0x00), QERGB(0x00, 0x00, 0x00),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_CURRENT_LINE, "current-line",
              COLOR_TRANSPARENT, QERGB(0x3f, 0x3f, 0x3f),
              0, 0, 1)

    /* default style for HTML/CSS2 pages */
    STYLE_DEF(QE_STYLE_CSS_DEFAULT, "css-default",
              QERGB(0x00, 0x00, 0x00), QERGB(0xbb, 0xbb, 0xbb),
              QE_FONT_FAMILY_SERIF, 12, 0)

    /* coloring styles */
    STYLE_DEF(QE_STYLE_HIGHLIGHT, "highlight",
              QERGB(0x00, 0x00, 0x00), QERGB(0xd8, 0xd8, 0xd8),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_SELECTION, "selection",
              QERGB(0xff, 0xff, 0xff), QERGB(0x00, 0x00, 0xff),
              0, 0, 0)

    /* Generic syntax coloring styles */
    STYLE_DEF(QE_STYLE_COMMENT, "comment",
              QERGB(0x00, 0xd8, 0xd8), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_PREPROCESS, "preprocess",
              QERGB(0xff, 0x00, 0xff), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_STRING, "string",
              QERGB(0xff, 0x00, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_STRING_Q, "string-q",
              QERGB(0xff, 0x00, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_KEYWORD, "keyword",
              QERGB(0xff, 0xff, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_NUMBER, "number",
              QERGB(0xff, 0x00, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_FUNCTION, "function",
              QERGB(0x00, 0x8f, 0x8f), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_VARIABLE, "variable",
              QERGB(0xcf, 0xcf, 0xcf), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_TYPE, "type",
              QERGB(0x00, 0xff, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_TAG, "tag",
              QERGB(0x00, 0xff, 0xff), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_CSS, "css",
              QERGB(0x98, 0xf8, 0x98), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_ERROR, "error",
              QERGB(0xff, 0x00, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)

    /* popup / region styles */
    STYLE_DEF(QE_STYLE_REGION_HILITE, "region-hilite",
              QERGB(0xff, 0xff, 0xff), QERGB(0x5c, 0x5c, 0xff),
              0, 0, 1)
    STYLE_DEF(QE_STYLE_SEARCH_HILITE, "search-hilite",
              QERGB(0xff, 0xff, 0xff), QERGB(0x00, 0x00, 0xd8),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_SEARCH_MATCH, "search-match",
              QERGB(0xff, 0xff, 0xff), QERGB(0x00, 0x00, 0xff),
              0, 0, 0)
    STYLE_DEF(QE_STYLE_BLANK_HILITE, "blank-hilite", /* black on red */
              QERGB(0x00, 0x00, 0x00), QERGB(0xff, 0x00, 0x00), 
	          0, 0, 0)


    /* HTML coloring styles */
    STYLE_DEF(QE_STYLE_HTML_COMMENT, "html-comment",
              QERGB(0xf8, 0x44, 0x00), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_HTML_STRING, "html-string",
              QERGB(0xf8, 0xa0, 0x78), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_HTML_ENTITY, "html-entity",
              QERGB(0xe8, 0xdc, 0x80), COLOR_TRANSPARENT,
              0, 0, 0)
    STYLE_DEF(QE_STYLE_HTML_TAG, "html-tag",
              QERGB(0x80, 0xcc, 0xf0), COLOR_TRANSPARENT,
              0, 0, 0)
