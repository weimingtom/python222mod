//20180406

#define SRE_MAGIC 20010701
#define SRE_OP_FAILURE 0
#define SRE_OP_SUCCESS 1
#define SRE_OP_ANY 2
#define SRE_OP_ANY_ALL 3
#define SRE_OP_ASSERT 4
#define SRE_OP_ASSERT_NOT 5
#define SRE_OP_AT 6
#define SRE_OP_BRANCH 7
#define SRE_OP_CALL 8
#define SRE_OP_CATEGORY 9
#define SRE_OP_CHARSET 10
#define SRE_OP_BIGCHARSET 11
#define SRE_OP_GROUPREF 12
#define SRE_OP_GROUPREF_IGNORE 13
#define SRE_OP_IN 14
#define SRE_OP_IN_IGNORE 15
#define SRE_OP_INFO 16
#define SRE_OP_JUMP 17
#define SRE_OP_LITERAL 18
#define SRE_OP_LITERAL_IGNORE 19
#define SRE_OP_MARK 20
#define SRE_OP_MAX_UNTIL 21
#define SRE_OP_MIN_UNTIL 22
#define SRE_OP_NOT_LITERAL 23
#define SRE_OP_NOT_LITERAL_IGNORE 24
#define SRE_OP_NEGATE 25
#define SRE_OP_RANGE 26
#define SRE_OP_REPEAT 27
#define SRE_OP_REPEAT_ONE 28
#define SRE_OP_SUBPATTERN 29
#define SRE_AT_BEGINNING 0
#define SRE_AT_BEGINNING_LINE 1
#define SRE_AT_BEGINNING_STRING 2
#define SRE_AT_BOUNDARY 3
#define SRE_AT_NON_BOUNDARY 4
#define SRE_AT_END 5
#define SRE_AT_END_LINE 6
#define SRE_AT_END_STRING 7
#define SRE_AT_LOC_BOUNDARY 8
#define SRE_AT_LOC_NON_BOUNDARY 9
#define SRE_AT_UNI_BOUNDARY 10
#define SRE_AT_UNI_NON_BOUNDARY 11
#define SRE_CATEGORY_DIGIT 0
#define SRE_CATEGORY_NOT_DIGIT 1
#define SRE_CATEGORY_SPACE 2
#define SRE_CATEGORY_NOT_SPACE 3
#define SRE_CATEGORY_WORD 4
#define SRE_CATEGORY_NOT_WORD 5
#define SRE_CATEGORY_LINEBREAK 6
#define SRE_CATEGORY_NOT_LINEBREAK 7
#define SRE_CATEGORY_LOC_WORD 8
#define SRE_CATEGORY_LOC_NOT_WORD 9
#define SRE_CATEGORY_UNI_DIGIT 10
#define SRE_CATEGORY_UNI_NOT_DIGIT 11
#define SRE_CATEGORY_UNI_SPACE 12
#define SRE_CATEGORY_UNI_NOT_SPACE 13
#define SRE_CATEGORY_UNI_WORD 14
#define SRE_CATEGORY_UNI_NOT_WORD 15
#define SRE_CATEGORY_UNI_LINEBREAK 16
#define SRE_CATEGORY_UNI_NOT_LINEBREAK 17
#define SRE_FLAG_TEMPLATE 1
#define SRE_FLAG_IGNORECASE 2
#define SRE_FLAG_LOCALE 4
#define SRE_FLAG_MULTILINE 8
#define SRE_FLAG_DOTALL 16
#define SRE_FLAG_UNICODE 32
#define SRE_FLAG_VERBOSE 64
#define SRE_INFO_PREFIX 1
#define SRE_INFO_LITERAL 2
#define SRE_INFO_CHARSET 4