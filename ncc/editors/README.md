# NihaoC 编辑器生态

为 NihaoC 语言提供的第三方编辑器/工具支持，按编辑器分子目录存放：

```
editors/
├── sublime/          Sublime Text 语法高亮 (nihaoc.sublime-syntax)
└── (vscode/          VS Code 插件，待添加)
    (vim/             Vim/Neovim 语法与插件，待添加)
```

## Sublime Text 安装

1. Sublime Text → Preferences → Browse Packages…，打开用户包目录
   （Windows 下通常是 `%APPDATA%\Sublime Text\Packages\`）
2. 在 Packages 目录下新建文件夹 `NihaoC/`
3. 把 `sublime/nihaoc.sublime-syntax` 复制到 `NihaoC/` 内
4. 重启 Sublime Text（或等待自动加载）

之后打开 `.nc` / `.nihao` / `.nh` 文件即自动启用 NihaoC 高亮。
如需语法调试：Ctrl+Shift+P → “Set Syntax: NihaoC”；
查看作用域：Ctrl+Shift+P → “Show Scope Name”（或 Ctrl+Alt+Shift+P）。
