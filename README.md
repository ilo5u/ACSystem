# ACSystem
酒店空调计费系统（服务器）
## 正在测试阶段
1. 基于Web服务器搭建
2. 使用Microsoft开源的cpprestsdk（使用时需注意在VS2017环境下编译时需修改三处地方的代码，并且该库中的web::json::value并未提供int64的导出，可以借助double进行转换）
