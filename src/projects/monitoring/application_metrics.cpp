//
// Created by getroot on 20. 1. 16.
//

#include "monitoring_private.h"
#include "application_metrics.h"
#include "host_metrics.h"

namespace mon
{
    ov::String ApplicationMetrics::GetInfoString(bool show_children)
    {
        ov::String out_str = ov::String::FormatString("\n[Application Info]\nid(%u), name(%s)\nCreated Time (%s)\n", 														
														GetId(), GetName().CStr(),
														ov::Converter::ToString(_created_time).CStr());
        

        out_str.Append(CommonMetrics::GetInfoString());

        if(show_children)
        {
            for(auto &t : _streams)
            {
                auto stream = t.second;
                out_str.Append(stream->GetInfoString());
            }
        }

        return out_str;
    }

    void ApplicationMetrics::ShowInfo(bool show_children)
    {
        logti("%s", GetInfoString(show_children).CStr());
    }

    bool ApplicationMetrics::OnStreamCreated(const info::Stream &stream)
    {
        std::unique_lock<std::shared_mutex> lock(_streams_guard);
        
        // If already stream metrics is exist,
        if(_streams.find(stream.GetId()) != _streams.end())
        {
            return true;
        }

        auto stream_metrics = std::make_shared<StreamMetrics>(GetSharedPtr(), stream);
        if(stream_metrics == nullptr)
        {
            logte("Cannot create StreamMetrics (%s/%s)", GetName().CStr(), stream.GetName().CStr());
            return false;
        }

        _streams[stream.GetId()] = stream_metrics;

        logti("Create StreamMetrics(%s) for monitoring", stream.GetName().CStr());
        return true;
    }

    bool ApplicationMetrics::OnStreamDeleted(const info::Stream &stream)
    {
        auto stream_metric = GetStreamMetrics(stream); 
        if(stream_metric == nullptr)
        {
            return false;
        }

        logti("Delete StreamMetrics(%s) for monitoring", stream.GetName().CStr());


		// If there are sessions in the stream, the number of visitors to the app is recalculated.
		// Calculate connections to application only if it hasn't origin stream to prevent double subtract. 
		if(stream_metric->GetOriginStream() == nullptr)
		{
			for(uint8_t type = static_cast<uint8_t>(PublisherType::Unknown); type < static_cast<uint8_t>(PublisherType::NumberOfPublishers); type++)
			{
				// Forward value to HostMetrics to sum
				GetHostMetrics()->OnSessionsDisconnected(static_cast<PublisherType>(type), stream_metric->GetConnections(static_cast<PublisherType>(type)));
				OnSessionsDisconnected(static_cast<PublisherType>(type), stream_metric->GetConnections(static_cast<PublisherType>(type)));
			}
		}

        // logging StreamMetric
        stream_metric->ShowInfo();

        std::unique_lock<std::shared_mutex> lock(_streams_guard);
        {
            _streams.erase(stream.GetId());
        }

        return true;
    }

    std::map<uint32_t, std::shared_ptr<StreamMetrics>> ApplicationMetrics::GetStreamMetricsMap()
    {
        std::shared_lock<std::shared_mutex> lock(_streams_guard);
        return _streams;
    }

    std::shared_ptr<StreamMetrics> ApplicationMetrics::GetStreamMetrics(const info::Stream &stream)
    {
        std::shared_lock<std::shared_mutex> lock(_streams_guard);
        if(_streams.find(stream.GetId()) == _streams.end())
        {
            return nullptr;
        }
        
        return _streams[stream.GetId()];
    }

	bool ApplicationMetrics::OnStreamReserved(ProviderType who, const ov::Url &stream_uri, const ov::String &stream_name)
	{
		auto reserved_stream_metric = std::make_shared<ReservedStreamMetrics>(who, stream_uri, stream_name);

		std::lock_guard<std::shared_mutex> lock(_reserved_streams_guard);
		_reserved_streams[stream_uri.Port()] = reserved_stream_metric;

		logti("%s has reserved %s stream linked to %s", StringFromProviderType(who).CStr(), stream_name.CStr(), stream_uri.ToUrlString().CStr());

		return true;
	}

	std::map<uint32_t, std::shared_ptr<ReservedStreamMetrics>> ApplicationMetrics::GetReservedStreamMetricsMap()
	{
		std::shared_lock<std::shared_mutex> lock(_reserved_streams_guard);
		return _reserved_streams;
	}

    std::map<uint32_t, std::shared_ptr<ReservedStreamMetrics>> ApplicationMetrics::GetReservedStreamMetricsMap() const
	{
		std::shared_lock<std::shared_mutex> lock(_reserved_streams_guard);
		return _reserved_streams;
	}

    void ApplicationMetrics::IncreaseBytesIn(uint64_t value)
    {
        // Forward value to HostMetrics to sum
		GetHostMetrics()->IncreaseBytesIn(value);
		CommonMetrics::IncreaseBytesIn(value);
    }

    void ApplicationMetrics::IncreaseBytesOut(PublisherType type, uint64_t value)
    {
        // Forward value to HostMetrics to sum
		GetHostMetrics()->IncreaseBytesOut(type, value);
		CommonMetrics::IncreaseBytesOut(type, value);
    }

    void ApplicationMetrics::OnSessionConnected(PublisherType type)
    {
         // Forward value to HostMetrics to sum
		GetHostMetrics()->OnSessionConnected(type);
		CommonMetrics::OnSessionConnected(type);
    }

    void ApplicationMetrics::OnSessionDisconnected(PublisherType type)
    {
        // Forward value to HostMetrics to sum
		GetHostMetrics()->OnSessionDisconnected(type);
		CommonMetrics::OnSessionDisconnected(type);
    }
}