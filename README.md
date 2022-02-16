# D3D12 Translation Layer

The D3D12 Translation Layer is a helper library for translating graphics concepts and commands from a D3D11-style domain to a D3D12-style domain.

A D3D11-style application generally:
* Records graphics commands in a single-threaded manner.
* Treats the CPU and GPU timeline as a single timeline. While there are some places where the asynchronicity of the GPU is exposed (e.g. queries or do-not-wait semantics), for the most part, a D3D11-style application can remain unaware of the fact that commands are recorded and executed at a later point in time.
  * Related to this is the fact that CPU-visible memory contents must be preserved from the time the CPU wrote them, until after the GPU has finished reading them in order to maintain the illusion of a single timeline.
* Creates individual state objects (e.g. blend state, rasterizer state) and compiles individual shaders, and only at the time when Draw is invoked does the application provide the full set of state which will be used.
* Ignore GPU parallelism and pipelining, trusting that driver introspection will maximize GPU utilization by parallelizing when possible, while preserving D3D11 semantics by synchronizing when necessary.

In contrast, a D3D12-style application must:
* Be aware of GPU asynchronicity, and manually synchronize the CPU and GPU.
* Be aware of GPU parallelism, and manually synchronize/barrier/transition resources from one usage to another.
* Manage memory, including allocation, deallocation, and "renaming" (discussed further below).
* Provide large bundles of state (called pipeline state objects in D3D12) all at once to enable cross-pipeline compilation and optimization.

To that end, this library provides an implementation of an API that looks like [D3D11](https://docs.microsoft.com/en-us/windows/win32/api/_direct3d11/), and submits work to [D3D12](https://docs.microsoft.com/en-us/windows/win32/api/_direct3d12/).

Make sure that you visit the [DirectX Landing Page](https://devblogs.microsoft.com/directx/landing-page/) for more resources for DirectX developers.

## Project Background

This project was started during the development of Windows 10 and D3D12. The Windows graphics team has a large set of D3D11 content which was heavily utilized during design and bringup of the D3D12 runtime and driver models. In order to use that content, a mapping layer, named D3D11On12, was developed.

This mapping layer proved successful and useful, to the point that a second mapping layer was developed, named D3D9On12. As the name implies, this maps from D3D9 to D3D12, and has to solve a lot of the same problems as D3D11On12. So, D3D11On12 was refactored into two pieces: a part that implements the D3D11-specific concepts, and a more general part that translates more traditional graphics constructs into a modern low-level D3D12 API consumer. This more general part is what became the D3D12TranslationLayer.

This code is currently being used by two mapping layers that ship as part of Windows: D3D11On12 and D3D9On12. In addition to the core D3D12TranslationLayer code, we also have released the source to D3D11On12, to serve as an example of how to consume this library.

## What does this do?

This translation layer provides the following high-level constructs (and more) for applications to use:

* Resource binding  
  The D3D12 resource binding model is quite different from D3D11 and prior. Rather than having a flat array of resources set on the pipeline which map 1:1 with shader registers, D3D12 takes a more flexible approach which is also closer to modern hardware. The translation layer takes care of figuring out which registers a shader needs, managing root signatures, populating descriptor heaps/tables, and setting up null descriptors for unbound resources.

* Resource renaming  
  D3D11 and older have a concept of `DISCARD` CPU access patterns, where the CPU populates a resource, instructs the GPU to read from it, and then immediately populates new contents without waiting for the GPU to read the old ones. This pattern is typically implemented via a pattern called "renaming", where new memory is allocated during the `DISCARD` operation, and all future references to that resource in the API will point to the new memory rather than the old. The translation layer provides a separation of a resource from its "identity," which enables cheap swapping of the underlying memory of a resource for that of another one without having to recreate views or rebind them. It also provides easy access to rename operations (allocate new memory with the same properties as the current, and swap their identities).

* Resource suballocation, pooling, and deferred destruction  
  D3D11-style apps can destroy objects immediately after instructing the GPU to do something with them. D3D12 requires applications to hold on to memory and GPU objects until the GPU has finished accessing them. Additionally, D3D11 apps suffer no penalty from allocating small resources (e.g. 16-byte buffers), where D3D12 apps must recognize that such small allocations are infeasible and should be suballocated from larger resources. Furthermore, constantly creating and destroying resources is a common pattern in D3D11, but in D3D12 this can quickly become expensive. The translation layer handles all of these abstractions seamlessly.

* Batching and threading  
  Since D3D11 patterns generally require applications to record all graphics commands on a single thread, there are often other CPU cores that are idle. To improve utilization, the translation layer provides a batching layer which can sit on top of the immediate context, moving the majority of work to a second thread so it can be parallelized. It also provides threadpool-based helpers for offloading PSO compilation to worker threads. Combining these means that compilations can be kicked off at draw-time on the application thread, and only the batching thread needs to wait for them to be completed. Meanwhile, other PSO compilations are starting or completing, minimizing the wall clock time spent compiling shaders.

* Residency management  
  This layer incorporates the open-source residency management library to improve utilization on low-memory systems.

## Building

This project produces a lib named D3D12TranslationLayer.lib. Additionally, if the [WDK](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) is installed in addition to the Windows SDK, a second project for a second lib named D3D12TranslationLayer_WDK.lib will be created.

The D3D12TranslationLayer project requires C++17, and only supports building with MSVC at the moment.

## Contributing

This project welcomes contributions. See [CONTRIBUTING](CONTRIBUTING.md) for more information. Contributions to this project will flow back to the D3D11On12 and D3D9On12 mapping layers included in Windows 10.

## Roadmap

There are three items currently on the roadmap:
1. Refactoring for additional componentization - currently the translation layer is largely implemented by a monolithic class called the `ImmediateContext`. It would be difficult for an application consumer to pick and choose bits and pieces of functionality provided by this class, but that would be desirable to ease application porting to D3D12 while enabling the application to take on only those responsibilities with which they can achieve improved performance through app-specific information.  
A high-level thinking here is to require consumers to have an `ImmediateContext` object, and have sub-components that are registered with that context. For example, the resource state tracker component need not always be present, and applications could provide explicit resource barriers rather than relying on the `ImmediateContext` to do it for them.  
A key constraint on this componentization is that it should not negatively impact performance.
2. Supporting initial data upload on `D3D12_COMMAND_LIST_TYPE_COPY` for discrete GPUs, and using `WriteToSubresource` for UMA (integrated) GPUs. This should improve performance.
3. Supporting multi-GPU scenarios, specifically, using multiple nodes on a single D3D12 device. Currently, the D3D12TranslationLayer only supports one node, though it can be a node other than node 0.

Other suggestions or contributions are welcome.

## Data Collection

The software may collect information about you and your use of the software and send it to Microsoft. Microsoft may use this information to provide services and improve our products and services. You may turn off the telemetry as described in the repository. There are also some features in the software that may enable you and Microsoft to collect data from users of your applications. If you use these features, you must comply with applicable law, including providing appropriate notices to users of your applications together with a copy of Microsoft's privacy statement. Our privacy statement is located at https://go.microsoft.com/fwlink/?LinkID=824704. You can learn more about data collection and use in the help documentation and our privacy statement. Your use of the software operates as your consent to these practices.

Specifically:  
The `g_hTracelogging` variable has events emitted against it with keywords which may trigger telemetry to be sent, depending on the configuration and registration of the tracelogging provider which is used. If no tracelogging provider is specified (using `D3D12TranslationLayer::SetTraceloggingProvider`) or if the specified provider is not configured for telemetry, then no telemetry will be sent. In the default configuration, no provider is created and no data is sent.
