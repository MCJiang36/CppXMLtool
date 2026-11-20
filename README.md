# CppXMLtool - 简体中文

单头文件的 C++17 XML 解析与序列化库.



* 单头文件：包含 `cppxmltool.h` 即可

* 命名空间: `mcjiang`

* 许可证: MIT

* 标准: C++17 或更高

* 版本: 1.1

## 快速开始



```
#include "cppxmltool.h"
#include <iostream>

int main() {
    mcjiang::xml doc("<root id=\"1\"><item>hello</item></root>");
    std::cout << doc.to_str() << std::endl;
    return 0;
}
```

## 解析

用字符串构造 `mcjiang::xml`; `load()` 用来重新解析并替换当前这棵树.



```
mcjiang::xml doc;
doc.load("<root><a>1</a><b>2</b></root>");
```

格式不对时抛 `std::runtime_error`, 消息里带字节偏移:



```
[CppXMLtool] parse error: mismatched closing tag at offset 17
```

### 支持的语法



* 元素: `<a></a>`, `<a/>`, `<a />`. 空元素按 `EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'` 解析，`/` 和 `>` 之间不能有空格

* 属性：单引号，双引号都行；属性之间要留空白；重名报错

* 文本

* 注释: `<!-- ... -->`, 放在根元素前后或元素内部都可以，解析后丢弃；内容里不能出现 `--`

* CDATA: `<![CDATA[ ... ]]>`, 内容按文本读入，CDATA 边界不保留

* 处理指令: `<? ... ?>`, 放在根元素前后或元素内部都可以，解析后丢弃；target 不能是保留名 `xml`, 文档开头的 XML 声明是唯一例外

* 实体：五个预定义实体，以及 `&#十进制;`, `&#x十六进制;`(只认小写 `x`, 解码为 UTF-8)

* 输入开头的 UTF-8 BOM 直接跳过

数字实体按 XML 1.0 校验：只收数字；尾随字符，代理区 U+D800-U+DFFF, `&#0;`, 超过 U+10FFFF 都报错.

DTD / DOCTYPE 不支持，遇到就报错.

### 严格性

解析器只接受良构的 XML 1.0 文档。下面这些都会报错:



* 名字不合 \[4] NameStartChar / \[5] NameChar (完整 Unicode 范围): 中文标签名可以，数字开头不行

* XML 声明不在文档最前，出现两次，版本不是 1.0, 缺 `version`, 或 `encoding` / `standalone` 写法不合规范

* 属性值里有裸 `<`

* 注释里有 `--`

* 文本里有 `]]>`

* 输入不是合法 UTF-8, 或含 XML Char 之外的控制字符 (0x00-0x08 等)

解析前还会先做两件事：按 §2.11 把 CRLF 和单独的 CR 归一成 LF; 属性值里的字面空白按 §3.3.3 归一成一个空格，字符引用 (如 `&#x09;`) 保持原值.

几条边界：只接受 UTF-8 输入，`encoding` 只解析不转码；只支持 1.0; 不处理命名空间，`ns:tag` 当普通名字.

## 访问节点

`xml::tag` 表示一个标签，包含:



* `name`: 标签名

* `attrs`: 属性表，`std::unordered_map<std::string, std::string>`

* `sub_tree`: 子节点，`std::vector<node>`

`xml::node` 是 `std::variant<std::string, std::unique_ptr<tag>>`, 用来区分文本和子标签.



```
mcjiang::xml doc("<root><item id=\"a\">hello</item></root>");

mcjiang::xml::tag& root = doc.root();
std::cout << root.name << std::endl; // root

mcjiang::xml::tag* item = doc.find("item");
if (item) {
    std::cout << item->attrs["id"] << std::endl; // a
}
```

`find()` 按文档顺序找第一个同名标签，找不到返回 `nullptr`; const 和非 const 两个重载行为一致.

## 遍历子节点

`range` 只遍历直接子节点:



```
mcjiang::xml::range r(doc.root());
for (auto& n : r) {
    if (mcjiang::xml::is_text(n)) {
        std::cout << "text: " << std::get<std::string>(n) << std::endl;
    } else if (mcjiang::xml::is_tag(n)) {
        auto& t = *std::get<std::unique_ptr<mcjiang::xml::tag>>(n);
        std::cout << "tag: " << t.name << std::endl;
    }
}
```

`recurse_range` 深度优先遍历整棵树:



```
mcjiang::xml::recurse_range rr(&doc.root());
for (auto& n : rr) {
    if (mcjiang::xml::is_tag(n)) {
        auto& t = *std::get<std::unique_ptr<mcjiang::xml::tag>>(n);
        std::cout << t.name << std::endl;
    }
}
```

`range` 和 `recurse_range` 都不能拷贝，可以移动。传 `const tag&` 会复制一份，传 `tag*` 借用原对象.

## 序列化

`to_str()` 输出完整文本，自带 XML 声明，特殊字符自动转义。属性按属性名排序，同一棵树每次输出的结果一样:



```
mcjiang::xml doc;
doc.load("<root a=\"1\">text &amp; more</root>");
std::cout << doc.to_str() << std::endl;
```

输出:



```
<?xml version="1.0" encoding="UTF-8"?>
<root a="1">text &amp; more</root>
```

默认构造的 `xml` 没有根节点，`to_str()` 返回空字符串，不会输出 `<>`. 解析时丢掉的注释和处理指令无法还原；自闭合标签统一写成 `<a></a>`.

## 统计与判断



```
doc.tag_count(); // 标签总数 (含根标签)
mcjiang::xml::is_text(node); // 是否为文本节点
mcjiang::xml::is_tag(node); // 是否为标签节点
```

## 修改树

`tag` 实现了深拷贝，直接拷贝，赋值标签，再用 `root()` 设置根节点:



```
mcjiang::xml::tag new_root;
new_root.name = "config";

mcjiang::xml doc;
doc.root(std::move(new_root));
```

## 注意事项



* 输入是一个完整文档，只能有一个根元素；根元素之后只允许空白，注释和处理指令

* 属性重名，闭合标签不匹配，实体没写完，都会报错

* 不支持 DTD / DOCTYPE

* 文本里的 `&` 会当作实体开头，要字面量就写 `&amp;`

* 注释，处理指令，CDATA 边界解析后不保留，这个库适合读写数据，不是无损 XML 编辑器

* `<a/ >` 不是合法的空元素写法，写成 `<a/>` 或 `<a />`

## 许可证

MIT 许可证，版权所有 (C) 2026 MCJiang



***

# CppXMLtool - English

A single-header C++17 XML parser and serializer.



* Header-only: include `cppxmltool.h`

* Namespace: `mcjiang`

* License: MIT

* Standard: C++17 or later

* Version: 1.1

## Quick start



```
#include "cppxmltool.h"
#include <iostream>

int main() {
    mcjiang::xml doc("<root id=\"1\"><item>hello</item></root>");
    std::cout << doc.to_str() << std::endl;
    return 0;
}
```

## Parsing

Construct an `mcjiang::xml` from a string; `load()` reparses and replaces the current tree.



```
mcjiang::xml doc;
doc.load("<root><a>1</a><b>2</b></root>");
```

Malformed input throws `std::runtime_error`, and the message carries a byte offset:



```
[CppXMLtool] parse error: mismatched closing tag at offset 17
```

### Supported syntax



* Elements: `<a></a>`, `<a/>`, `<a />`. An empty element follows `EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'`, so no space is allowed between `/` and `>`

* Attributes: single or double quotes; whitespace is required between attributes; duplicate names are rejected

* Text

* Comments: `<!-- ... -->`, allowed before the root, after it, or inside elements, dropped after parsing; `--` may not appear inside

* CDATA: `<![CDATA[ ... ]]>`, read as text, the CDATA boundary is not kept

* Processing instructions: `<? ... ?>`, allowed before the root, after it, or inside elements, dropped after parsing; the target may not be the reserved name `xml`, and the XML declaration at the start of the document is the only exception

* Entities: the five predefined ones, plus `&#decimal;` and `&#xhex;` (lowercase `x` only, decoded to UTF-8)

* A leading UTF-8 BOM is skipped

Numeric character references follow XML 1.0: digits only; trailing characters, surrogates U+D800–U+DFFF, `&#0;`, and values above U+10FFFF all raise an error.

DTD / DOCTYPE is not supported and raises an error.

### Strictness

The parser accepts only well-formed XML 1.0 documents. The following raise an error:



* A name outside \[4] NameStartChar / \[5] NameChar (full Unicode ranges): Chinese tag names work, names starting with a digit do not

* The XML declaration not at the very start of the document, appearing twice, carrying a version other than 1.0, missing `version`, or using malformed `encoding` / `standalone`

* A raw `<` inside an attribute value

* `--` inside a comment

* `]]>` inside text

* Input that is not valid UTF-8, or contains characters outside XML Char (0x00–0x08 and similar)

Two things happen before parsing: CRLF and a lone CR are normalized to LF per §2.11; literal whitespace in attribute values is normalized to one space per §3.3.3, while character references such as `&#x09;` keep their value.

A few boundaries: UTF-8 input only, and `encoding` is parsed but no transcoding is performed; XML 1.0 only; namespace rules are not applied, and `ns:tag` is treated as an ordinary name.

## Accessing nodes

`xml::tag` represents an element and contains:



* `name`: tag name

* `attrs`: attribute map, `std::unordered_map<std::string, std::string>`

* `sub_tree`: child nodes, `std::vector<node>`

`xml::node` is `std::variant<std::string, std::unique_ptr<tag>>`, used to distinguish text from child elements.



```
mcjiang::xml doc("<root><item id=\"a\">hello</item></root>");

mcjiang::xml::tag& root = doc.root();
std::cout << root.name << std::endl; // root

mcjiang::xml::tag* item = doc.find("item");
if (item) {
    std::cout << item->attrs["id"] << std::endl; // a
}
```

`find()` returns the first element with the given name in document order, or `nullptr` if not found; the const and non-const overloads behave identically.

## Iterating children

`range` iterates the direct children only:



```
mcjiang::xml::range r(doc.root());
for (auto& n : r) {
    if (mcjiang::xml::is_text(n)) {
        std::cout << "text: " << std::get<std::string>(n) << std::endl;
    } else if (mcjiang::xml::is_tag(n)) {
        auto& t = *std::get<std::unique_ptr<mcjiang::xml::tag>>(n);
        std::cout << "tag: " << t.name << std::endl;
    }
}
```

`recurse_range` traverses the whole tree depth-first:



```
mcjiang::xml::recurse_range rr(&doc.root());
for (auto& n : rr) {
    if (mcjiang::xml::is_tag(n)) {
        auto& t = *std::get<std::unique_ptr<mcjiang::xml::tag>>(n);
        std::cout << t.name << std::endl;
    }
}
```

`range` and `recurse_range` cannot be copied but can be moved. Passing a `const tag&` makes a copy; passing a `tag*` borrows the original object.

## Serializing

`to_str()` outputs the full text with an XML declaration and escapes special characters. Attributes are sorted by name, so the same tree produces the same output every time:



```
mcjiang::xml doc;
doc.load("<root a=\"1\">text &amp; more</root>");
std::cout << doc.to_str() << std::endl;
```

Output:



```
<?xml version="1.0" encoding="UTF-8"?>
<root a="1">text &amp; more</root>
```

A default-constructed `xml` has no root, so `to_str()` returns an empty string instead of `<>`. Comments and processing instructions dropped during parsing cannot be restored; self-closing tags are written as `<a></a>`.

## Counting and checking



```
doc.tag_count(); // total number of tags (including root)
mcjiang::xml::is_text(node); // is a text node
mcjiang::xml::is_tag(node); // is a tag node
```

## Modifying the tree

`tag` is deep-copyable; copy or assign tags directly, then set the root with `root()`:



```
mcjiang::xml::tag new_root;
new_root.name = "config";

mcjiang::xml doc;
doc.root(std::move(new_root));
```

## Notes



* The input is one complete document with a single root element; after the root only whitespace, comments and processing instructions are allowed

* Duplicate attributes, mismatched closing tags and unterminated entities all raise an error

* DTD / DOCTYPE is not supported

* A literal `&` in text starts an entity; write `&amp;` for the character itself

* Comments, processing instructions and CDATA boundaries are dropped after parsing; this library reads and writes data and is not a lossless XML editor

* `<a/ >` is not a legal empty-element tag; write `<a/>` or `<a />`

## License

MIT License, Copyright (C) 2026 MCJiang